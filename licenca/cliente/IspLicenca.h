#pragma once

// Licença do pacote (um e-mail + um token por pacote, até 2 computadores). Uma instância por plugin carregado
// (SharedResourcePointer); o arquivo da licença é do pacote, então ativar num plugin libera os outros do mesmo pacote
// (cada um percebe pelo arquivo, ver timerCallback).
//
// Definições que o CMake passa (licenca/cliente/licenca.cmake): ISP_PACOTE, ISP_LICENCA_URL,
// ISP_LICENCA_CHAVE (chave pública Ed25519 em hex).
//
// Arquivo: Windows %APPDATA%\InsideStudioSP\Licencas\<pacote>.lic · Mac ~/Library/Application Support/InsideStudioSP/Licencas
//   { email, token, dados, assinatura }  — dados/assinatura vêm do servidor (ver licenca/servidor/src/index.js)
// A licença assinada vale 45 dias; com mais de 3 dias o plugin confirma de novo pela internet, em segundo plano.
// Com a licença valendo, ao carregar ele pergunta uma vez ao servidor se há versão nova do pacote (GET /v1/versao);
// a TelaAtivacao mostra o aviso. ISP_VERSAO_ATUAL (opcional) finge outra versão, só para testar o aviso.
//
// Ativação AUTOMÁTICA (24/11/2026): o token é um só por e-mail e vale para todos os pacotes da pessoa. Pacote sem licença
// neste computador procura o e-mail+token gravado por OUTRO plugin InsideStudioSP (os .lic desta pasta e as licenças dos
// devices Max for Live) e tenta ativar sozinho, com "auto": true no pedido. Deu certo = a pessoa nem vê a tela de ativação.
// Não tem este pacote = a tela abre com o e-mail já preenchido e um aviso. Cada conjunto de candidatos é tentado UMA vez
// (<pacote>.auto guarda o hash): abrir o DAW de novo não repete o pedido, e DESATIVAR não é desfeito na próxima abertura.
//
// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.

#include <juce_gui_basics/juce_gui_basics.h>
#include "IspEd25519.h"
#if ISP_ATUALIZAR
 #include "IspAtualizacao.h"
#endif

// ISP_LICENCA_FOTO: só o workflow shots, que compila a tela como programa comum (sem JucePlugin_*, sem isp_licenca())
#ifdef ISP_LICENCA_FOTO
 #define ISP_NOME_PLUGIN   "foto"
 #define ISP_VERSAO_PLUGIN "0.0.0"
 #define ISP_PACOTE        "foto"
 #define ISP_LICENCA_URL   ""
 #define ISP_LICENCA_CHAVE ""
#else
 #define ISP_NOME_PLUGIN   JucePlugin_Name
 #define ISP_VERSAO_PLUGIN JucePlugin_VersionString
#endif

#ifndef ISP_VERSAO_ATUAL
 #define ISP_VERSAO_ATUAL ISP_VERSAO_PLUGIN
#endif

#ifndef ISP_PACOTE
 #error "ISP_PACOTE não definido: use isp_licenca() no CMakeLists do plugin"
#endif

namespace isp
{
// textos com acento no fonte (UTF-8)
inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

class Licenca : public juce::ChangeBroadcaster, private juce::Timer
{
public:
    static constexpr const char* pacote = ISP_PACOTE;

    // nome que aparece na tela
    static juce::String nomePacote()
    {
        const juce::String p (pacote);
        if (p == "criativo") return "CRIATIVO";
        if (p == "mix")      return "MIX";
        if (p == "knob")     return utf8 ("SÉRIE KNOB");
        if (p == "quimera")  return "QUIMERA";
        if (p == "limao")    return utf8 ("LIMÃO");
        if (p == "mercurio") return utf8 ("MERCÚRIO");
        if (p == "resenha")  return "RESENHA";
        if (p == "montamonta") return "MONTA-MONTA";
        if (p == "groovador") return "GROOVADOR";
        if (p == "mesa") return "MESA";
        if (p == "align") return "ALIGN";
        if (p == "barebass") return "BAREBASS";
        if (p == "stompbox") return "STOMPBOX";
        if (p == "maestro") return "MAESTRO";
        if (p == "magos") return "MASTER DOS MAGOS";
        if (p == "gogo") return utf8 ("GOGÓ");
        if (p == "retalho") return "RETALHO";
        if (p == "chute") return "CHUTE";
        if (p == "curva") return "CURVA";
        return p.toUpperCase();
    }

    Licenca()
    {
       #ifdef ISP_LICENCA_FOTO   // só o workflow shots (foto da tela fora do plugin): liberado, sem arquivo nem internet
        ok = true;
       #else
        carregar();
        startTimer (3000);
       #endif
    }

    ~Licenca() override
    {
        stopTimer();
        pool.removeAllJobs (true, 3000);
    }

    // thread de áudio: true = toca normalmente
    bool liberada() const noexcept { return ok.load (std::memory_order_relaxed); }

    // ---- thread da interface ----
    bool ocupada() const noexcept { return emAndamento; }
    juce::String mensagem() const { return msg; }
    juce::String email() const { return emailAtual; }
    juce::String token() const { return tokenAtual; }
    juce::Time validaAte() const { return juce::Time ((juce::int64) validaAteSeg * 1000); }

    // versão nova do pacote (vazia = está em dia, ou ainda não perguntou, ou a pessoa dispensou o aviso)
    static juce::String versaoAtual() { return ISP_VERSAO_ATUAL; }
    juce::String versaoNova() const { return novaVersao != dispensada ? novaVersao : juce::String(); }
    juce::String linkVersaoNova() const { return urlNova; }
    // "Agora não": some em todos os plugins do pacote abertos; volta na próxima vez que o DAW carregar o plugin
    void dispensarAviso() { dispensada = novaVersao; sendChangeMessage(); }

    // atualização automática: só nos plugins com ISP_ATUALIZAR e quando o servidor mandou o instalador assinado
   #if ISP_ATUALIZAR
    Atualizador* atualizador() { return atualiza.disponivel() ? &atualiza : nullptr; }
    Atualizador& motorAtualizacao() { return atualiza; }   // para ouvir as mudanças (addChangeListener)
   #else
    void* atualizador() { return nullptr; }
   #endif

    // "1.10.0" > "1.9.2"
    static int compararVersao (const juce::String& a, const juce::String& b)
    {
        auto x = juce::StringArray::fromTokens (a.trim(), ".", {});
        auto y = juce::StringArray::fromTokens (b.trim(), ".", {});
        for (int i = 0; i < 3; ++i)
        {
            const int p = x[i].getIntValue(), q = y[i].getIntValue();
            if (p != q) return p < q ? -1 : 1;
        }
        return 0;
    }

    void ativar (const juce::String& email, const juce::String& token)
    {
        if (emAndamento) return;
        msg = utf8 ("Ativando…");
        candidatos.clear();
        pedir ("ativar", email.trim().toLowerCase(), token.trim().toUpperCase(), true);
    }

    // libera a vaga deste computador no servidor e apaga a licença daqui
    void desativar()
    {
        if (emAndamento || tokenAtual.isEmpty()) return;
        msg = utf8 ("Desativando…");
        pedir ("desativar", emailAtual, tokenAtual, true);
    }

    static juce::File arquivo()
    {
       #if JUCE_MAC
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
       #else
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #endif
        return base.getChildFile ("InsideStudioSP/Licencas").getChildFile (juce::String (pacote) + ".lic");
    }

    // id deste computador (hash; o id bruto não sai daqui)
    static juce::String maquina()
    {
        auto id = juce::SystemStats::getUniqueDeviceID();
        if (id.isEmpty())
            id = juce::SystemStats::getComputerName() + "|" + juce::SystemStats::getLogonName();
        const auto txt = ("InsideStudioSP|" + id).toStdString();
        std::uint8_t h[64];
        ed25519::sha512 (h, txt.data(), txt.size());
        return juce::String::toHexString (h, 32, 0);
    }

private:
    // confere a licença gravada; liga/desliga ok e decide se precisa confirmar pela internet
    void carregar()
    {
        const auto f = arquivo();
        ultimaData = f.getLastModificationTime();
        const auto lido = juce::JSON::parse (f.loadFileAsString());
        emailAtual = lido["email"].toString();
        tokenAtual = lido["token"].toString();

        juce::String erro;
        const bool valida = conferir (lido, erro);
        ok = valida;
        if (valida)
        {
            msg = {};
            if (! versaoPerguntada)
            {
                versaoPerguntada = true;
                perguntarVersao();
            }
            const auto agora = juce::Time::currentTimeMillis() / 1000;
            if (! jaConfirmou && agora - emitidaSeg > 3 * 86400)
            {
                jaConfirmou = true;
                pedir ("validar", emailAtual, tokenAtual, false);
            }
        }
        else if (f.existsAsFile() && tokenAtual.isNotEmpty() && ! jaConfirmou)
        {
            // vencida (ou de outra máquina): tenta confirmar sozinha antes de pedir os dados
            jaConfirmou = true;
            msg = erro;
            pedir ("validar", emailAtual, tokenAtual, false);
        }
        else if (f.existsAsFile())
            msg = erro;
        else if (! tentouAuto)
        {
            tentouAuto = true;
            tentarAutomatica();
        }
        sendChangeMessage();
    }

    // ---- ativação automática com o e-mail+token de outro plugin InsideStudioSP deste computador
    struct Candidato { juce::String email, token; juce::Time quando; };

    static juce::String normToken (const juce::String& t)
    {
        return t.toUpperCase().retainCharacters ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    // { email, token } de uma licença gravada: .lic dos VST (JSON) ou dos devices M4L (.txt = JSON; .json = { "v": JSON })
    static void lerCandidato (const juce::File& arq, juce::Array<Candidato>& lista)
    {
        if (! arq.existsAsFile() || arq.getSize() > 64 * 1024) return;
        auto j = juce::JSON::parse (arq.loadFileAsString());
        if (j.isObject() && j["v"].isString() && ! j["email"].isString())
            j = juce::JSON::parse (j["v"].toString());
        const auto email = j["email"].toString().trim().toLowerCase();
        const auto token = j["token"].toString().trim().toUpperCase();
        if (! email.containsChar ('@') || normToken (token).length() < 20) return;
        for (auto& c : lista)
            if (c.email == email && normToken (c.token) == normToken (token))
            {
                c.quando = juce::jmax (c.quando, arq.getLastModificationTime());
                return;
            }
        lista.add ({ email, token, arq.getLastModificationTime() });
    }

    static juce::Array<Candidato> procurarCandidatos()
    {
        juce::Array<Candidato> lista;
        const auto pasta = arquivo().getParentDirectory();
        const juce::String meu = juce::String (pacote) + ".lic";
        for (const auto& e : juce::RangedDirectoryIterator (pasta, false, "*.lic", juce::File::findFiles))
            if (e.getFile().getFileName() != meu)
                lerCandidato (e.getFile(), lista);
        // devices Max for Live: pasta comum fixa e a pasta do usuário (isp_licenca.js)
       #if JUCE_MAC
        const juce::File m4l[] { juce::File ("/Users/Shared"), pasta.getParentDirectory().getParentDirectory() };
       #else
        const juce::File m4l[] { juce::File ("C:\\Users\\Public\\Documents"), pasta.getParentDirectory().getParentDirectory() };
       #endif
        for (const auto& p : m4l)
            for (const auto& e : juce::RangedDirectoryIterator (p, false, "InsideStudioSP-*-licenca.*", juce::File::findFiles))
                lerCandidato (e.getFile(), lista);
        std::sort (lista.begin(), lista.end(), [] (const Candidato& a, const Candidato& b) { return a.quando > b.quando; });
        while (lista.size() > 3) lista.removeLast();
        return lista;
    }

    static juce::File arquivoAuto() { return arquivo().withFileExtension ("auto"); }

    static juce::String hashCandidatos (const juce::Array<Candidato>& lista)
    {
        juce::StringArray partes;
        for (const auto& c : lista) partes.add (c.email + "|" + normToken (c.token));
        partes.sort (false);
        const auto txt = ("InsideStudioSP|auto|" + juce::String (pacote) + "|" + partes.joinIntoString (",")).toStdString();
        std::uint8_t h[64];
        ed25519::sha512 (h, txt.data(), txt.size());
        return juce::String::toHexString (h, 16, 0);
    }

    // estes candidatos já foram tentados (ou a pessoa desativou com eles por aqui): não pede de novo
    static void marcarTentados (const juce::Array<Candidato>& lista)
    {
        if (lista.isEmpty()) return;
        const auto f = arquivoAuto();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (hashCandidatos (lista));
    }

    void tentarAutomatica()
    {
        candidatos = procurarCandidatos();
        if (candidatos.isEmpty()) return;
        if (arquivoAuto().loadFileAsString().trim() == hashCandidatos (candidatos))
        {
            candidatos.clear();
            return;
        }
        candidatosTentados = candidatos;
        autoIndice = 0;
        msg = utf8 ("Procurando a licença dos seus outros plugins InsideStudioSP…");
        pedir ("ativar", candidatos[0].email, candidatos[0].token, false, true);
    }

    // a tentativa automática não deu: próximo candidato, ou a tela com o e-mail já preenchido
    void automaticaFalhou (const juce::String& erro, bool respondeu)
    {
        if (respondeu && ++autoIndice < candidatos.size())
        {
            pedir ("ativar", candidatos[autoIndice].email, candidatos[autoIndice].token, false, true);
            return;
        }
        if (respondeu) marcarTentados (candidatosTentados);   // sem internet: tenta de novo na próxima abertura
        if (emailAtual.isEmpty() && candidatosTentados.size() > 0) emailAtual = candidatosTentados[0].email;
        msg = erro == "pacote_nao_incluido"
                ? utf8 ("Seu e-mail já tem outros plugins InsideStudioSP, mas não o ") + nomePacote()
                      + utf8 (". Se você comprou ou pegou este, digite o token que chegou no e-mail.")
                : juce::String();
        candidatos.clear();
        sendChangeMessage();
    }

    static void chavePublica (std::uint8_t pk[32])
    {
        const juce::String chave (ISP_LICENCA_CHAVE);
        for (int i = 0; i < 32; ++i)
            pk[i] = (std::uint8_t) chave.substring (i * 2, i * 2 + 2).getHexValue32();
    }

    // { dados, assinatura } do instalador vindo do /v1/versao: devolve os dados só se a assinatura confere
    static juce::var instaladorAssinado (const juce::var& inst)
    {
        juce::MemoryOutputStream dados, assinatura;
        if (! inst.isObject()
            || ! juce::Base64::convertFromBase64 (dados, inst["dados"].toString())
            || ! juce::Base64::convertFromBase64 (assinatura, inst["assinatura"].toString())
            || assinatura.getDataSize() != 64)
            return {};
        std::uint8_t pk[32];
        chavePublica (pk);
        if (! ed25519::verificar (pk, static_cast<const std::uint8_t*> (assinatura.getData()), dados.getData(), dados.getDataSize()))
            return {};
        return juce::JSON::parse (dados.toUTF8());
    }

    bool conferir (const juce::var& lido, juce::String& erro)
    {
        if (! lido.isObject()) return false;
        juce::MemoryOutputStream dados, assinatura;
        if (! juce::Base64::convertFromBase64 (dados, lido["dados"].toString())
            || ! juce::Base64::convertFromBase64 (assinatura, lido["assinatura"].toString())
            || assinatura.getDataSize() != 64)
        {
            erro = utf8 ("Licença danificada. Ative de novo.");
            return false;
        }

        std::uint8_t pk[32];
        chavePublica (pk);

        if (! ed25519::verificar (pk, static_cast<const std::uint8_t*> (assinatura.getData()), dados.getData(), dados.getDataSize()))
        {
            erro = utf8 ("Licença danificada. Ative de novo.");
            return false;
        }

        const auto d = juce::JSON::parse (dados.toUTF8());
        if ((int) d["v"] != 1 || d["pacote"].toString() != pacote)
        {
            erro = utf8 ("Licença de outro pacote.");
            return false;
        }
        if (d["maquina"].toString() != maquina())
        {
            erro = utf8 ("A licença gravada é de outro computador. Ative neste.");
            return false;
        }
        emitidaSeg = (juce::int64) d["emitida"];
        validaAteSeg = (juce::int64) d["valida_ate"];
        if (juce::Time::currentTimeMillis() / 1000 > validaAteSeg)
        {
            erro = utf8 ("A licença precisa ser confirmada pela internet (a cada 45 dias). Conecte o computador e clique em ATIVAR.");
            return false;
        }
        emailAtual = d["email"].toString();
        return true;
    }

    // chamada ao servidor numa thread à parte; a resposta volta para a thread da interface
    void pedir (const juce::String& acao, const juce::String& email, const juce::String& token, bool mostrarErro,
                bool automatica = false)
    {
        emAndamento = true;
        sendChangeMessage();

        auto* corpo = new juce::DynamicObject();
        corpo->setProperty ("pacote", pacote);
        corpo->setProperty ("email", email);
        corpo->setProperty ("token", token);
        corpo->setProperty ("maquina", maquina());
        corpo->setProperty ("nome_maquina", juce::SystemStats::getComputerName());
        corpo->setProperty ("versao", juce::String (ISP_NOME_PLUGIN) + " " + ISP_VERSAO_PLUGIN);
        if (automatica) corpo->setProperty ("auto", true);
        const auto json = juce::JSON::toString (juce::var (corpo), true);
        const juce::String url = juce::String (ISP_LICENCA_URL) + "/v1/" + acao;

        juce::WeakReference<Licenca> eu (this);
        pool.addJob ([eu, acao, email, token, json, url, mostrarErro, automatica]
        {
            int status = 0;
            juce::String texto;
            auto opcoes = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                              .withExtraHeaders ("Content-Type: application/json")
                              .withConnectionTimeoutMs (12000)
                              .withStatusCode (&status);
            if (auto in = juce::URL (url).withPOSTData (json).createInputStream (opcoes))
                texto = in->readEntireStreamAsString();
            const auto resp = juce::JSON::parse (texto);

            juce::MessageManager::callAsync ([eu, acao, email, token, resp, mostrarErro, automatica]
            {
                if (auto* l = eu.get())
                    l->resposta (acao, email, token, resp, mostrarErro, automatica);
            });
        });
    }

    // tem versão nova do pacote? (sem internet ou servidor fora: não avisa nada)
    void perguntarVersao()
    {
       #if JUCE_MAC
        const juce::String sistema ("mac");
       #else
        const juce::String sistema ("win");
       #endif
        const auto url = juce::URL (juce::String (ISP_LICENCA_URL) + "/v1/versao")
                             .withParameter ("pacote", pacote)
                             .withParameter ("versao", versaoAtual())
                             .withParameter ("sistema", sistema)
                            #if ISP_CANAL_TESTE   // build de teste: o servidor também oferece os pré-lançamentos
                             .withParameter ("teste", "1")
                            #endif
                             ;

        juce::WeakReference<Licenca> eu (this);
        pool.addJob ([eu, url, sistema]
        {
            juce::String texto;
            auto opcoes = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (12000);
            if (auto in = url.createInputStream (opcoes))
                texto = in->readEntireStreamAsString();
            const auto resp = juce::JSON::parse (texto);

            juce::MessageManager::callAsync ([eu, resp, sistema]
            {
                auto* l = eu.get();
                const auto nova = resp["atualizacao"];
                if (l == nullptr || ! (bool) resp["ok"] || ! nova.isObject()) return;
                const auto v = nova["versao"].toString(), link = nova["url"].toString();
                if (compararVersao (v, versaoAtual()) <= 0 || ! link.startsWithIgnoreCase ("https://")) return;
                l->novaVersao = v;
                l->urlNova = link;
               #if ISP_ATUALIZAR
                // instalador assinado pelo servidor: só vale se é deste pacote, deste sistema e desta versão
                const auto d = instaladorAssinado (nova["instalador"]);
                const auto sha = d["sha256"].toString(), baixar = d["url"].toString();
                if ((int) d["v"] == 1 && d["tipo"].toString() == "instalador" && d["pacote"].toString() == pacote
                    && d["sistema"].toString() == sistema && d["versao"].toString() == v
                    && sha.length() == 64 && sha.containsOnly ("0123456789abcdef")
                    && baixar.startsWithIgnoreCase ("https://"))
                    l->atualiza.preparar (pacote, nomePacote(), v, baixar, sha, (juce::int64) d["tamanho"]);
               #else
                juce::ignoreUnused (sistema);
               #endif
                l->sendChangeMessage();
            });
        });
    }

    void resposta (const juce::String& acao, const juce::String& email, const juce::String& token,
                   const juce::var& resp, bool mostrarErro, bool automatica = false)
    {
        emAndamento = false;
        const auto f = arquivo();

        // ativação automática que não deu: segue quieta (sem mensagem de erro do servidor)
        if (automatica && ! (resp.isObject() && (bool) resp["ok"]))
        {
            automaticaFalhou (resp["erro"].toString(), resp.isObject());
            return;
        }

        if (! resp.isObject())
        {
            // sem internet / servidor fora: se a licença ainda vale, segue tudo como está
            if (mostrarErro || ! ok)
                msg = utf8 ("Não consegui falar com o servidor de licenças. Confira a internet e tente de novo.");
            sendChangeMessage();
            return;
        }

        if ((bool) resp["ok"])
        {
            if (acao == "desativar")
            {
                marcarTentados (procurarCandidatos());   // não reativa sozinho na próxima abertura
                f.deleteFile();
                ok = false;
                emailAtual = tokenAtual = {};
                msg = utf8 ("Este computador foi desativado. A vaga ficou livre para outro.");
                ultimaData = {};
                sendChangeMessage();
                return;
            }
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("email", email);
            obj->setProperty ("token", token);
            obj->setProperty ("dados", resp["licenca"]["dados"]);
            obj->setProperty ("assinatura", resp["licenca"]["assinatura"]);
            f.getParentDirectory().createDirectory();
            f.replaceWithText (juce::JSON::toString (juce::var (obj)));
            carregar();
            if (! ok && msg.isEmpty())
                msg = utf8 ("A resposta do servidor não conferiu. Tente de novo.");
            if (ok && acao == "ativar")
                msg = utf8 (automatica ? "Ativado sozinho com a licença dos seus outros plugins InsideStudioSP. Bom som!" : "Ativado. Bom som!");
            if (automatica) candidatos.clear();
            sendChangeMessage();
            return;
        }

        const auto erro = resp["erro"].toString();
        msg = resp["mensagem"].toString();
        // a licença deixou de valer no servidor: apaga a cópia daqui
        if (acao == "validar" && (erro == "revogada" || erro == "nao_encontrada" || erro == "maquina_nao_ativada"))
        {
            f.deleteFile();
            ok = false;
            ultimaData = {};
        }
        sendChangeMessage();
    }

    // outro plugin do mesmo pacote ativou/desativou: o arquivo muda e este percebe
    void timerCallback() override
    {
        if (emAndamento) return;
        const auto f = arquivo();
        const auto data = f.existsAsFile() ? f.getLastModificationTime() : juce::Time();
        if (data != ultimaData)
            carregar();
    }

    std::atomic<bool> ok { false };
    bool emAndamento = false, jaConfirmou = false, versaoPerguntada = false, tentouAuto = false;
    juce::Array<Candidato> candidatos, candidatosTentados;
    int autoIndice = 0;
    juce::String msg, emailAtual, tokenAtual, novaVersao, urlNova, dispensada;
    juce::int64 emitidaSeg = 0, validaAteSeg = 0;
    juce::Time ultimaData;
    juce::ThreadPool pool { 1 };
   #if ISP_ATUALIZAR
    Atualizador atualiza;
   #endif

    JUCE_DECLARE_WEAK_REFERENCEABLE (Licenca)
    JUCE_DECLARE_NON_COPYABLE (Licenca)
};
} // namespace isp
