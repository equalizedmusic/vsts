#include "PluginEditor.h"

using namespace vacina;
using vac::RF;

namespace
{
constexpr int sizeMenuBase = 10000;   // ids do submenu "Tamanho da janela"
constexpr int menuSaveAs = 20001, menuOverwrite = 20002, menuRename = 20003, menuDelete = 20004, menuNoUser = 20005,
              menuImport = 20006, menuRemoveBank = 30000;   // menuRemoveBank + n = remove o banco importado n

// posições fixas desenhadas no painel (o timer repinta só elas)
const juce::Point<float> lfoLedPos { 506.0f, 60.0f };
const juce::Point<float> midiLedPos { 434.0f, 470.0f };
const juce::Point<float> syncLedPos { 434.0f, 490.0f };
const juce::Rectangle<int> meterArea { 830, 440, 64, 110 };
const juce::Rectangle<int> statusArea { 420, 458, 80, 60 };
} // namespace

// =====================================================================
// VISOR
// =====================================================================
void LcdDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (6.0f);
    vac::lcdGlass (g, r, 2.0f);
    const auto in = r.reduced (10.0f, 6.0f);
    vac::lcdText (g, l1, in.withHeight (14.0f), 10.5f, juce::Justification::centredLeft);
    g.setColour (vac::lcdInk.withAlpha (0.5f));
    g.fillRect (in.getX(), in.getY() + 16.0f, in.getWidth(), 0.8f);
    vac::lcdText (g, l2, in.withTrimmedTop (18.0f).withHeight (28.0f), 19.0f, juce::Justification::centredLeft);
    vac::lcdText (g, l3, in.withTrimmedTop (48.0f).withHeight (16.0f), 11.0f, juce::Justification::centredLeft);
    vac::lcdFinish (g, r, 2.0f);
}

// =====================================================================
// EDITOR
// =====================================================================
vac::VKnob* VacinaEditor::knob (int cx, int y, int size, const juce::String& pid, const juce::String& label,
                                juce::Colour pointer, const juce::String& sub)
{
    const int w = juce::jmax (size + 8, label.length() > 11 ? 80 : 58);   // nome comprido (MASTER VOLUME) não corta
    return add<vac::VKnob> ({ cx - w / 2, y, w, size + 8 + (sub.isNotEmpty() ? 22 : 12) }, proc.apvts, pid, label, size, pointer, sub);
}

vac::Key* VacinaEditor::key (int x, int y, const juce::String& label, bool led)
{
    // a caixa começa padX antes da tecla: x é onde a tecla aparece
    return add<vac::Key> ({ x - (int) vac::Key::padX, y, 50 + (led ? 14 : 0), label.isNotEmpty() ? 28 : 16 }, label, led);
}

void VacinaEditor::attach (juce::Button& b, const juce::String& paramId)
{
    b.setClickingTogglesState (true);
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, b));
}

// tecla que passa para a próxima opção da lista (volta ao começo no fim)
void VacinaEditor::cycle (const juce::String& paramId)
{
    auto* p = proc.apvts.getParameter (paramId);
    const int steps = juce::jmax (2, p->getNumSteps());
    const int idx = (juce::roundToInt (p->getValue() * (float) (steps - 1)) + 1) % steps;
    p->beginChangeGesture();
    p->setValueNotifyingHost ((float) idx / (float) (steps - 1));
    p->endChangeGesture();
}

void VacinaEditor::showPage (Pages& p, int index)
{
    p.current = juce::jlimit (0, (int) p.items.size() - 1, index);
    for (size_t i = 0; i < p.items.size(); ++i)
        for (auto* c : p.items[i])
            c->setVisible ((int) i == p.current);
    if (p.leds != nullptr) p.leds->setIndex (p.current);
}

VacinaEditor::VacinaEditor (VacinaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    canvas.onPaint = [this] (juce::Graphics& g) { drawCanvas (g); };
    canvas.setBounds (0, 0, vac::baseW, vac::baseH);
    addAndMakeVisible (canvas);

    auto& st = proc.apvts;
    const auto red = vac::red;

    // páginas: cada lista de LEDs troca o que aparece e a tecla SELECT passa para a próxima
    auto pageLeds = [this] (Pages& pg, juce::Rectangle<int> r, juce::StringArray names, bool vertical, float step)
    {
        pg.leds = add<vac::LedList> (r, names, vertical, step);
        pg.leds->onSelect = [this, &pg] (int i) { showPage (pg, i); };
        pg.items.resize ((size_t) names.size());
    };
    auto selectKey = [this] (Pages& pg, int x, int y)
    {
        key (x, y, "SELECT")->onClick = [this, &pg] { nextPage (pg); };
    };

    // ================= MASTER, MATRIX, ARP =================
    knob (92, 70, 46, id::volume, "MASTER VOLUME");

    for (int k = 0; k < numSlots; ++k)
    {
        const int y = 56 + k * 28;
        add<vac::LcdBox> ({ 160, y, 72, 24 }, st, id::mSrc (k));
        add<vac::LcdBox> ({ 236, y, 72, 24 }, st, id::mDst (k));
        add<vac::VKnob> ({ 308, y - 3, 30, 30 }, st, id::mAmt (k), "", 22, red);
    }

    attach (*key (60, 182, "ARP ON", true), id::arpOn);
    attach (*key (60, 222, "HOLD", true), id::arpHold);
    add<vac::LcdBox> ({ 56, 258, 52, 24 }, st, id::arpOct);
    auto* arpMode = add<vac::LedList> ({ 122, 178, 74, 78 }, arpModeNames, true, 13.0f);
    arpMode->bind (st, id::arpMode);
    key (126, 262, "MODE")->onClick = [this] { cycle (id::arpMode); };
    knob (238, 184, 40, id::arpRate, "RATE");
    knob (296, 184, 40, id::arpGate, "GATE");

    // ================= MODULATORS (LFO 1 / LFO 2) =================
    selectKey (lfoPages, 348, 58);
    pageLeds (lfoPages, { 386, 52, 70, 26 }, { "LFO 1", "LFO 2" }, false, 34.0f);
    auto* sh1 = add<vac::LedList> ({ 512, 50, 52, 78 }, lfoShapeNames, true, 13.0f);
    auto* sh2 = add<vac::LedList> ({ 512, 50, 52, 78 }, lfoShapeNames, true, 13.0f);
    sh1->bind (st, id::lfo1Shape);
    sh2->bind (st, id::lfo2Shape);
    auto* shk1 = key (516, 132, "SHAPE");
    auto* shk2 = key (516, 132, "SHAPE");
    shk1->onClick = [this] { cycle (id::lfo1Shape); };
    shk2->onClick = [this] { cycle (id::lfo2Shape); };
    lfoPages.items[0] = { knob (470, 84, 44, id::lfo1Rate, "RATE"), add<vac::LcdBox> ({ 346, 98, 76, 24 }, st, id::lfo1Sync), sh1, shk1,
                          knob (368, 186, 40, id::lfo1Pitch, "OSC PITCH"), knob (422, 186, 40, id::lfo1Cut, "CUTOFF"),
                          knob (476, 186, 40, id::lfo1Pw, "PULSE W"), knob (530, 186, 40, id::lfo1Amp, "AMP") };
    lfoPages.items[1] = { knob (470, 84, 44, id::lfo2Rate, "RATE"), add<vac::LcdBox> ({ 346, 98, 76, 24 }, st, id::lfo2Sync), sh2, shk2,
                          knob (368, 186, 40, id::lfo2Shp, "OSC SHAPE"), knob (422, 186, 40, id::lfo2Fm, "FM"),
                          knob (476, 186, 40, id::lfo2Cut, "CUTOFF"), knob (530, 186, 40, id::lfo2Pan, "PAN") };

    // ================= OSCILLATORS =================
    selectKey (oscPages, 836, 58);
    pageLeds (oscPages, { 868, 52, 42, 28 }, { "OSC 1", "OSC 2" }, true, 13.0f);
    auto* model = add<vac::LedList> ({ 868, 94, 46, 28 }, modelNames, true, 13.0f);
    model->bind (st, id::osc1Model);
    auto* modelKey = key (836, 100, "MODEL");
    modelKey->onClick = [this] { cycle (id::osc1Model); };
    auto* sync = key (836, 100, "SYNC", true);
    attach (*sync, id::osc2Sync);
    oscPages.items[0] = { knob (606, 54, 46, id::osc1Shape, "SHAPE"), knob (666, 54, 46, id::osc1Pw, "PULSE WIDTH"),
                          knob (726, 54, 46, id::osc1Semi, "SEMITONE"), knob (786, 54, 46, id::osc1Spread, "SPREAD", vac::silk, "SUPER"),
                          model, modelKey };
    oscPages.items[1] = { knob (606, 54, 46, id::osc2Shape, "SHAPE"), knob (666, 54, 46, id::osc2Pw, "PULSE WIDTH"),
                          knob (726, 54, 46, id::osc2Semi, "SEMITONE"), knob (786, 54, 46, id::osc2Detune, "DETUNE"), sync };

    key (588, 188, "KEY MODE")->onClick = [this] { cycle (id::keyMode); };
    add<vac::LedList> ({ 624, 180, 50, 40 }, keyModeNames, true, 13.0f)->bind (st, id::keyMode);
    knob (716, 178, 44, id::porta, "PORTAMENTO");
    knob (790, 178, 44, id::fmAmt, "FM AMOUNT", red, "OSC 1 > OSC 2");

    // ================= MIX =================
    knob (961, 54, 46, id::oscBal, "OSC BALANCE");
    knob (961, 178, 44, id::subVol, "SUB OSC");
    knob (961, 326, 46, id::oscVol, "OSC VOLUME", vac::silk, "SATURATION");
    knob (961, 452, 44, id::noiseVol, "NOISE VOLUME");

    // ================= FILTERS =================
    knob (1050, 50, 52, id::cutoff, "CUTOFF", red);
    knob (1050, 178, 44, id::cutoff2, "CUTOFF 2", vac::silk, "OFFSET");
    knob (1278, 54, 44, id::filtBal, "BALANCE", vac::silk, "FILT 1 / 2");
    selectKey (filtPages, 1094, 186);
    pageLeds (filtPages, { 1128, 180, 44, 28 }, { "FILT 1", "FILT 2" }, true, 13.0f);
    auto* m1 = add<vac::LedList> ({ 1216, 176, 46, 66 }, f1ModeNames, true, 13.0f);
    auto* m2 = add<vac::LedList> ({ 1216, 176, 46, 66 }, f2ModeNames, true, 13.0f);
    m1->bind (st, id::f1Mode);
    m2->bind (st, id::f2Mode);
    auto* mk1 = key (1182, 186, "MODE");
    auto* mk2 = key (1182, 186, "MODE");
    mk1->onClick = [this] { cycle (id::f1Mode); };
    mk2->onClick = [this] { cycle (id::f2Mode); };
    filtPages.items[0] = { knob (1112, 54, 44, id::reso1, "RESONANCE"), knob (1166, 54, 44, id::env1, "ENV AMOUNT"),
                           knob (1222, 54, 44, id::key1, "KEY FOLLOW"), m1, mk1 };
    filtPages.items[1] = { knob (1112, 54, 44, id::reso2, "RESONANCE"), knob (1166, 54, 44, id::env2, "ENV AMOUNT"),
                           knob (1222, 54, 44, id::key2, "KEY FOLLOW"), m2, mk2 };
    key (1268, 186, "ROUTING")->onClick = [this] { cycle (id::routing); };
    add<vac::LedList> ({ 1270, 218, 34, 28 }, routingNames, true, 13.0f)->bind (st, id::routing);

    // ================= EFFECTS =================
    selectKey (fxUpPages, 66, 334);
    pageLeds (fxUpPages, { 104, 328, 60, 40 }, { "DELAY", "REVERB", "EQ" }, true, 13.0f);
    fxUpPages.items[0] = { knob (212, 326, 44, id::dlySend, "SEND"), knob (280, 326, 44, id::dlyTime, "TIME", vac::silk, "TEMPO"),
                           knob (348, 326, 44, id::dlyFb, "FEEDBACK") };
    fxUpPages.items[1] = { knob (212, 326, 44, id::rvbSend, "SEND"), knob (280, 326, 44, id::rvbSize, "SIZE"),
                           knob (348, 326, 44, id::rvbDamp, "DAMPING") };
    fxUpPages.items[2] = { knob (212, 326, 44, id::eqLow, "LOW", vac::silk, "150 Hz"), knob (280, 326, 44, id::eqMid, "MID", vac::silk, "1.2 kHz"),
                           knob (348, 326, 44, id::eqHigh, "HIGH", vac::silk, "6 kHz") };

    selectKey (fxLoPages, 66, 452);
    pageLeds (fxLoPages, { 104, 446, 80, 54 }, { "DISTORTION", "ANALOG BOOST", "CHORUS", "PHASER" }, true, 13.0f);
    fxLoPages.items[0] = { knob (246, 446, 44, id::distType, "TYPE"), knob (322, 446, 44, id::distAmt, "INTENSITY", red) };
    fxLoPages.items[1] = { knob (246, 446, 44, id::boostAmt, "INTENSITY", red), knob (322, 446, 44, id::boostTune, "TUNE") };
    fxLoPages.items[2] = { knob (246, 446, 44, id::choMix, "MIX"), knob (322, 446, 44, id::choInt, "INTENSITY") };
    fxLoPages.items[3] = { knob (246, 446, 44, id::phaMix, "MIX"), knob (322, 446, 44, id::phaInt, "INTENSITY") };

    // ================= CENTRO: visor, presets, unison =================
    lcd = add<LcdDisplay> ({ 498, 342, 312, 98 });
    lcd->onClick = [this] { showPresetMenu(); };
    key (438, 352, "BROWSE")->onClick = [this] { showPresetMenu(); };
    key (438, 396, "STORE")->onClick = [this] { askNewPreset(); };
    key (832, 352, "PREV")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() - 1); };
    key (832, 396, "NEXT")->onClick = [this] { proc.loadPreset (proc.getPresetIndex() + 1); };
    knob (566, 456, 44, id::unison, "UNISON", vac::silk, "VALUE 1");
    knob (654, 456, 44, id::uniDetune, "DETUNE", vac::silk, "VALUE 2");
    knob (742, 456, 44, id::uniSpread, "PAN SPREAD", vac::silk, "VALUE 3");

    // ================= ENVELOPES =================
    const char* const envNames[5] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VELOCITY" };
    const juce::String fIds[5] = { id::fA, id::fD, id::fS, id::fR, id::fVel };
    const juce::String aIds[5] = { id::aA, id::aD, id::aS, id::aR, id::aVel };
    for (int i = 0; i < 5; ++i)
    {
        knob (1048 + i * 56, 322, 42, fIds[i], envNames[i]);
        knob (1048 + i * 56, 452, 42, aIds[i], envNames[i]);
    }

    for (auto* pg : { &oscPages, &lfoPages, &filtPages, &fxUpPages, &fxLoPages })
        showPage (*pg, 0);

    // janela redimensionável pelo canto, sempre na mesma proporção; o tamanho fica salvo no projeto.
    // o tamanho salvo é lido ANTES do setResizeLimits: ele encolhe a janela ainda vazia para o mínimo
    // e o resized() gravaria 75% por cima do que o usuário escolheu
    openScale = proc.uiScale.load();
    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) vac::baseW * vac::minScale), juce::roundToInt ((float) vac::baseH * vac::minScale),
                     juce::roundToInt ((float) vac::baseW * vac::maxScale), juce::roundToInt ((float) vac::baseH * vac::maxScale));
    getConstrainer()->setFixedAspectRatio ((double) vac::baseW / (double) vac::baseH);
    setScale (openScale);
    startTimerHz (30);
}

VacinaEditor::~VacinaEditor()
{
    dialog.close();
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------- menu de presets (com o tamanho da janela) ----------------
void VacinaEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = proc.getPresetNames();
    const auto bankNames = proc.getBankNames();
    const int  cur = proc.getPresetIndex();
    const int  numBanks = bankNames.size();

    // Init no topo; cada banco (FACTORY 1 e os importados, embaixo dele) abre as categorias; os do usuário em USER.
    // A categoria numCategories é OUTROS (preset de banco importado sem categoria).
    std::vector<std::array<juce::PopupMenu, numCategories + 1>> cats ((size_t) numBanks);
    std::vector<std::array<bool, numCategories + 1>> catTicked ((size_t) numBanks);
    juce::PopupMenu user;
    bool userTicked = false;
    for (int i = 0; i < names.size() && i + 1 < sizeMenuBase; ++i)
    {
        const int  c = proc.getPresetCategory (i);
        const int  b = proc.getPresetBank (i);
        const bool ticked = i == cur;
        if (c < 0)
            menu.addItem (i + 1, names[i], true, ticked);
        else if (b == VacinaProcessor::bankUser)
        {
            user.addItem (i + 1, names[i], true, ticked);
            userTicked = userTicked || ticked;
        }
        else if (b >= 0 && b < numBanks)
        {
            const auto k = (size_t) juce::jlimit (0, numCategories, c);
            cats[(size_t) b][k].addItem (i + 1, names[i], true, ticked);
            catTicked[(size_t) b][k] = catTicked[(size_t) b][k] || ticked;
        }
    }
    menu.addSeparator();
    for (int b = 0; b < numBanks; ++b)
    {
        juce::PopupMenu bank;
        bool bankTicked = false;
        for (int c = 0; c <= numCategories; ++c)
        {
            auto& sub = cats[(size_t) b][(size_t) c];
            if (sub.getNumItems() == 0) continue;
            const bool t = catTicked[(size_t) b][(size_t) c];
            bank.addSubMenu (c < numCategories ? u8 (categoryNames[c]) : juce::String ("OUTROS"), sub, true,
                             std::unique_ptr<juce::Drawable>(), t);
            bankTicked = bankTicked || t;
        }
        if (bank.getNumItems() > 0)
            menu.addSubMenu (bankNames[b].toUpperCase(), bank, true, std::unique_ptr<juce::Drawable>(), bankTicked);
    }
    if (user.getNumItems() > 0)
        menu.addSubMenu ("USER", user, true, std::unique_ptr<juce::Drawable>(), userTicked);
    else
        menu.addItem (menuNoUser, "USER (nenhum ainda)", false);

    // presets do usuário dá para gravar por cima, renomear e apagar; os de fábrica e os dos bancos não mudam
    menu.addSeparator();
    menu.addItem (menuSaveAs, u8 ("Salvar como novo preset…"));
    if (proc.isUserPreset())
    {
        const auto name = proc.getPresetName();
        menu.addItem (menuOverwrite, u8 ("Salvar alterações em “") + name + u8 ("”"));
        menu.addItem (menuRename,    u8 ("Renomear “") + name + u8 ("”…"));
        menu.addItem (menuDelete,    u8 ("Apagar “") + name + u8 ("”…"));
    }

    // bancos avulsos (.vcnbank)
    menu.addSeparator();
    menu.addItem (menuImport, u8 ("Importar banco de presets…"));
    if (numBanks > 1)
    {
        juce::PopupMenu remove;
        for (int b = 1; b < numBanks; ++b)
            remove.addItem (menuRemoveBank + b, bankNames[b].toUpperCase());
        menu.addSubMenu ("Remover banco", remove);
    }

    juce::PopupMenu sizes;
    const float current = proc.uiScale.load();
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (sizeMenuBase + pct, juce::String (pct) + "%", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    menu.addSeparator();
    menu.addSubMenu ("Tamanho da janela", sizes);

    // o menu (e cada camada: banco → categoria → preset) abre DENTRO da janela do plugin: como janela solta,
    // a camada de dentro ficava por baixo da janela do plugin (que o host deixa sempre na frente) e não dava clique
    juce::Component::SafePointer<VacinaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lcd).withParentComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->presetMenuChosen (result);
    });
}

void VacinaEditor::askNewPreset()
{
    juce::Component::SafePointer<VacinaEditor> safe (this);
    dialog.askName (canvas, "Salvar preset", proc.suggestPresetName(),
                    [safe] (const juce::String& name) { if (safe != nullptr) safe->proc.saveUserPreset (name); });
}

// abre o navegador de arquivos; o banco escolhido é copiado para a pasta do VACINA e entra no menu
void VacinaEditor::importBank()
{
    auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    if (start.getChildFile ("Downloads").isDirectory()) start = start.getChildFile ("Downloads");
    chooser = std::make_unique<juce::FileChooser> ("Importar banco de presets do VACINA", start, "*.vcnbank");

    juce::Component::SafePointer<VacinaEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safe == nullptr || ! file.existsAsFile()) return;
                              juce::String name;
                              const auto err = safe->proc.importBank (file, name);
                              if (err.isNotEmpty())
                                  safe->dialog.inform (safe->canvas, u8 ("Não deu para importar"), err);
                              else
                                  safe->dialog.inform (safe->canvas, "Banco importado",
                                                       u8 ("“") + name.toUpperCase() + u8 ("” já está no menu de presets, embaixo do FACTORY 1."));
                          });
}

void VacinaEditor::presetMenuChosen (int result)
{
    if (result > sizeMenuBase && result < menuSaveAs)
    {
        setScale ((float) (result - sizeMenuBase) / 100.0f);
        return;
    }

    juce::Component::SafePointer<VacinaEditor> safe (this);
    if (result > menuRemoveBank)
    {
        const int  bank = result - menuRemoveBank;
        const auto bankName = proc.getBankNames()[bank].toUpperCase();
        dialog.confirm (canvas, "Remover banco",
                        u8 ("“") + bankName + u8 ("” vai para a lixeira e sai do menu. Os presets do USER não mudam."), "Remover",
                        [safe, bank] { if (safe != nullptr) safe->proc.removeBank (bank); });
        return;
    }

    const auto name = proc.getPresetName();
    switch (result)
    {
        case menuSaveAs:
            askNewPreset();
            break;
        case menuOverwrite:
            proc.overwriteUserPreset();
            break;
        case menuRename:
            dialog.askName (canvas, "Renomear preset", name, [safe] (const juce::String& newName)
            {
                if (safe != nullptr && ! safe->proc.renameUserPreset (newName))
                    safe->dialog.inform (safe->canvas, u8 ("Não deu para renomear"), u8 ("Já existe um preset com esse nome."));
            });
            break;
        case menuDelete:
            dialog.confirm (canvas, "Apagar preset",
                            u8 ("“") + name + u8 ("” vai para a lixeira. O som atual continua no plugin."), "Apagar",
                            [safe] { if (safe != nullptr) safe->proc.deleteUserPreset(); });
            break;
        case menuImport:
            importBank();
            break;
        case menuNoUser:
            break;
        default:
            if (result > 0 && result < sizeMenuBase)
                proc.loadPreset (result - 1);
            break;
    }
}

// ---------------- timer (30 Hz) ----------------
void VacinaEditor::timerCallback()
{
    // o host pode reabrir a janela no tamanho da primeira vez: passado o meio segundo de abertura,
    // volta para o tamanho salvo e daí em diante qualquer mudança é gravada
    if (openTicks < openSettle && ++openTicks == openSettle
        && std::abs ((float) getWidth() / (float) vac::baseW - openScale) > 0.005f)
        setScale (openScale);

    // visor: preset e, por 2,5 s, o último parâmetro mexido (nome e valor)
    const int touch = proc.touchCount.load();
    if (touch != lastTouch) { lastTouch = touch; touchTimer = proc.lastTouched.load() >= 0 ? 75 : 0; }
    if (touchTimer > 0) --touchTimer;
    juce::String bottom;
    const auto& params = proc.getParameters();
    const int idx = proc.lastTouched.load();
    if (touchTimer > 0 && idx >= 0 && idx < params.size())
        bottom = params[idx]->getName (40).toUpperCase() + ": " + params[idx]->getCurrentValueAsText();
    else
        bottom = "VOICES " + juce::String (proc.synth.activeVoices.load()).paddedLeft ('0', 2) + "/16   "
               + juce::String (proc.shownBpm.load(), 1) + " BPM";
    lcd->set ("SINGLE  " + proc.getPresetGroup(), proc.getPresetName(), bottom);

    // LED do LFO (pisca no ritmo do LFO da página aberta)
    const float lfo = lfoPages.current == 0 ? proc.synth.lfo1Now.load() : proc.synth.lfo2Now.load();
    if (std::abs (lfo - lfoShown) > 0.03f)
    {
        lfoShown = lfo;
        canvas.repaint (juce::Rectangle<int> ((int) lfoLedPos.x - 12, (int) lfoLedPos.y - 12, 24, 24));
    }

    // MIDI e SYNC
    const int midi = proc.midiActivity.load();
    if (midi != lastMidi) { lastMidi = midi; midiGlow = 4; }
    const bool lit = midiGlow > 0;
    if (midiGlow > 0) --midiGlow;
    const bool syn = proc.synced.load();
    const float bpm = proc.shownBpm.load();
    if (lit != midiLit || syn != lastSync || std::abs (bpm - lastBpm) > 0.05f)
    {
        midiLit = lit;
        lastSync = syn;
        lastBpm = bpm;
        canvas.repaint (statusArea);
    }

    levelL = juce::jmax (proc.meterL.exchange (0.0f), levelL * 0.85f);
    levelR = juce::jmax (proc.meterR.exchange (0.0f), levelR * 0.85f);
    if (std::abs (levelL - shownL) > 0.002f || std::abs (levelR - shownR) > 0.002f)
    {
        shownL = levelL;
        shownR = levelR;
        canvas.repaint (meterArea);
    }
}

// ---------------- tamanho ----------------
void VacinaEditor::setScale (float scale)
{
    scale = juce::jlimit (vac::minScale, vac::maxScale, scale);
    setSize (juce::roundToInt ((float) vac::baseW * scale), juce::roundToInt ((float) vac::baseH * scale));
}

void VacinaEditor::resized()
{
    const float scale = (float) getWidth() / (float) vac::baseW;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (openTicks >= openSettle)   // enquanto a janela abre, o tamanho não é escolha do usuário
        proc.uiScale = scale;
}

void VacinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0e));
}

// ---------------- o aparelho ----------------
// logo: frasco de vacina (tampa, gargalo e o líquido com a gota) + o nome
void VacinaEditor::drawLogo (juce::Graphics& g, float x, float y, float s)
{
    const auto c = vac::silk;
    juce::Path vial;
    vial.addRoundedRectangle (x + 2.0f * s, y + 6.0f * s, 12.0f * s, 16.0f * s, 2.5f * s);
    g.setColour (c);
    g.strokePath (vial, juce::PathStrokeType (1.6f * s));
    g.fillRoundedRectangle (x + 1.0f * s, y, 14.0f * s, 4.5f * s, 1.0f * s);          // tampa
    g.fillRect (x + 4.0f * s, y + 4.0f * s, 8.0f * s, 2.5f * s);                      // gargalo
    g.setColour (vac::red);
    g.fillRoundedRectangle (x + 3.6f * s, y + 13.0f * s, 8.8f * s, 7.4f * s, 1.5f * s);  // o líquido
    juce::Path drop;
    drop.startNewSubPath (x + 8.0f * s, y + 8.0f * s);
    drop.quadraticTo (x + 10.5f * s, y + 11.5f * s, x + 8.0f * s, y + 12.4f * s);
    drop.quadraticTo (x + 5.5f * s, y + 11.5f * s, x + 8.0f * s, y + 8.0f * s);
    g.fillPath (drop);

    g.setFont (theme::sans (17.0f * s, true, 0.06f));
    g.setColour (real::black (0.6f));
    g.drawText ("VACINA", RF (x + 21.0f * s, y + 1.0f * s, 120.0f * s, 22.0f * s), juce::Justification::centredLeft, false);
    g.setColour (c);
    g.drawText ("VACINA", RF (x + 21.0f * s, y, 120.0f * s, 22.0f * s), juce::Justification::centredLeft, false);
}

void VacinaEditor::drawCanvas (juce::Graphics& g)
{
    const float W = (float) vac::baseW, H = (float) vac::baseH;
    g.fillAll (juce::Colour (0xff0d0d0e));

    // chapa grafite entre as duas laterais de madeira
    vac::panel (g, { 34.0f, 10.0f, W - 68.0f, H - 20.0f }, 4.0f);
    vac::woodCheek (g, { 2.0f, 2.0f, 40.0f, H - 4.0f }, true);
    vac::woodCheek (g, { W - 42.0f, 2.0f, 40.0f, H - 4.0f }, false);

    // conectores da traseira, serigrafados na beirada de cima
    {
        const char* const jacks[] = { "USB", "MIDI IN", "MIDI OUT", "IN L", "IN R", "OUT 1 L", "OUT 1 R", "OUT 2 L", "OUT 2 R",
                                      "OUT 3 L", "OUT 3 R", "12V DC IN", "PHONES" };
        float x = 360.0f;
        for (auto* j : jacks)
        {
            vac::silkText (g, j, { x, 15.0f, 64.0f, 9.0f }, juce::Justification::centred, 6.5f, true, vac::silkDim);
            x += 72.0f;
        }
    }

    drawLogo (g, 54.0f, 26.0f, 1.0f);

    // ---- títulos e marcações ----
    vac::sectionTitle (g, { 162.0f, 34.0f, 172.0f, 14.0f }, "MATRIX");
    for (int k = 0; k < numSlots; ++k)
        vac::silkText (g, juce::String (k + 1), { 148.0f, 56.0f + (float) k * 28.0f, 12.0f, 24.0f }, juce::Justification::centred, 8.0f);
    vac::silkText (g, "SOURCE", { 160.0f, 140.0f, 72.0f, 10.0f }, juce::Justification::centred, 7.0f, true, vac::silkDim);
    vac::silkText (g, "DESTINATION", { 236.0f, 140.0f, 72.0f, 10.0f }, juce::Justification::centred, 7.0f, true, vac::silkDim);
    vac::silkText (g, "AMOUNT", { 300.0f, 140.0f, 46.0f, 10.0f }, juce::Justification::centred, 7.0f, true, vac::silkDim);

    vac::sectionTitle (g, { 50.0f, 160.0f, 284.0f, 14.0f }, "ARP");
    vac::silkText (g, "OCTAVES", { 52.0f, 283.0f, 60.0f, 10.0f }, juce::Justification::centred, 7.0f);

    vac::sectionTitle (g, { 340.0f, 34.0f, 228.0f, 14.0f }, "MODULATORS");
    vac::silkText (g, "CLOCK", { 346.0f, 123.0f, 76.0f, 10.0f }, juce::Justification::centred, 7.0f);
    vac::silkText (g, "DESTINATIONS", { 346.0f, 164.0f, 210.0f, 10.0f }, juce::Justification::centredLeft, 7.0f, true, vac::redInk);
    {
        const float lfo = lfoPages.current == 0 ? proc.synth.lfo1Now.load() : proc.synth.lfo2Now.load();
        real::led (g, lfoLedPos, 2.6f, vac::red, 0.5f + 0.5f * lfo);
    }

    vac::sectionTitle (g, { 576.0f, 34.0f, 334.0f, 14.0f }, "OSCILLATORS");

    vac::sectionTitle (g, { 916.0f, 34.0f, 96.0f, 14.0f }, "MIX");
    // seta do "V" entre as metades do MIX (detalhe do painel)
    {
        juce::Path v;
        v.addTriangle (946.0f, 262.0f, 976.0f, 262.0f, 961.0f, 290.0f);
        g.setColour (real::black (0.5f));
        g.fillPath (v, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (vac::silkDim.withAlpha (0.55f));
        g.fillPath (v);
        g.setColour (juce::Colour (0xff2a2b2e));
        g.fillPath (v, juce::AffineTransform::scale (0.55f, 0.55f, 961.0f, 270.0f));
    }

    vac::sectionTitle (g, { 1016.0f, 34.0f, 296.0f, 14.0f }, "FILTERS");

    vac::sectionTitle (g, { 50.0f, 300.0f, 354.0f, 14.0f }, "EFFECTS");
    vac::groupFrame (g, { 56.0f, 320.0f, 342.0f, 104.0f });
    vac::groupFrame (g, { 56.0f, 436.0f, 342.0f, 112.0f });

    vac::sectionTitle (g, { 1016.0f, 300.0f, 296.0f, 14.0f }, "FILTER ENVELOPE");
    vac::sectionTitle (g, { 1016.0f, 430.0f, 296.0f, 14.0f }, "AMPLIFIER ENVELOPE");

    // placa do modelo em cima do visor
    {
        const RF plate (560.0f, 312.0f, 188.0f, 22.0f);
        g.setGradientFill (real::vertical (juce::Colour (0xff3b3c40), plate.getY(), juce::Colour (0xff1c1d1f), plate.getBottom()));
        g.fillRoundedRectangle (plate, 3.0f);
        g.setColour (real::white (0.12f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 3.0f, 0.8f);
        g.setFont (theme::sans (13.0f, true, 0.3f));
        g.setColour (vac::silk);
        g.drawText ("VACINA", plate.withTrimmedRight (60.0f), juce::Justification::centredRight, false);
        g.setColour (vac::red);
        g.fillRect (plate.getRight() - 56.0f, plate.getY() + 5.0f, 1.2f, 12.0f);
        g.setFont (theme::sans (8.0f, true, 0.2f));
        g.setColour (vac::silkDim);
        g.drawText ("DESKTOP", RF (plate.getRight() - 52.0f, plate.getY(), 48.0f, plate.getHeight()), juce::Justification::centredLeft, false);
    }

    // estado: MIDI, SYNC e BPM
    real::led (g, midiLedPos, 2.6f, vac::red, midiLit ? 1.0f : 0.0f);
    vac::silkText (g, "MIDI", { midiLedPos.x + 7.0f, midiLedPos.y - 5.0f, 40.0f, 10.0f }, juce::Justification::centredLeft, 7.0f);
    real::led (g, syncLedPos, 2.6f, vac::red, lastSync ? 1.0f : 0.0f);
    vac::silkText (g, "SYNC", { syncLedPos.x + 7.0f, syncLedPos.y - 5.0f, 40.0f, 10.0f }, juce::Justification::centredLeft, 7.0f);
    vac::silkText (g, juce::String (proc.shownBpm.load(), 1) + " BPM", { 424.0f, 503.0f, 74.0f, 10.0f },
                   juce::Justification::centredLeft, 7.0f, true, vac::silkDim);

    drawMeter (g);
}

// medidor de saída em LEDs (pico), dois canais
void VacinaEditor::drawMeter (juce::Graphics& g)
{
    const int leds = 12;
    const float levels[2] = { levelL, levelR };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (levels[c], -60.0f);
        const float v  = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -42.0f, 0.0f, 0.0f, 1.0f));
        for (int i = 0; i < leds; ++i)
        {
            const float f = (float) i / (float) (leds - 1);
            const auto col = f > 0.85f ? vac::red : f > 0.6f ? theme::amber : theme::leaf;
            real::ledBar (g, { (float) meterArea.getX() + 14.0f + (float) c * 18.0f, (float) meterArea.getBottom() - 22.0f - (float) i * 7.0f, 10.0f, 4.0f },
                          col, levels[c] > 0.0005f && f <= v ? 1.0f : 0.0f, 0.4f);
        }
    }
    vac::silkText (g, "L     R", { (float) meterArea.getX(), (float) meterArea.getBottom() - 14.0f, (float) meterArea.getWidth(), 10.0f },
                   juce::Justification::centred, 7.0f);
}
