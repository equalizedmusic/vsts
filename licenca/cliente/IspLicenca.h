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

#include <juce_gui_basics/juce_gui_basics.h>
#include "IspEd25519.h"

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
        sendChangeMessage();
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
        const juce::String chave (ISP_LICENCA_CHAVE);
        for (int i = 0; i < 32; ++i)
            pk[i] = (std::uint8_t) chave.substring (i * 2, i * 2 + 2).getHexValue32();

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
    void pedir (const juce::String& acao, const juce::String& email, const juce::String& token, bool mostrarErro)
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
        const auto json = juce::JSON::toString (juce::var (corpo), true);
        const juce::String url = juce::String (ISP_LICENCA_URL) + "/v1/" + acao;

        juce::WeakReference<Licenca> eu (this);
        pool.addJob ([eu, acao, email, token, json, url, mostrarErro]
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

            juce::MessageManager::callAsync ([eu, acao, email, token, resp, mostrarErro]
            {
                if (auto* l = eu.get())
                    l->resposta (acao, email, token, resp, mostrarErro);
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
                             .withParameter ("sistema", sistema);

        juce::WeakReference<Licenca> eu (this);
        pool.addJob ([eu, url]
        {
            juce::String texto;
            auto opcoes = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (12000);
            if (auto in = url.createInputStream (opcoes))
                texto = in->readEntireStreamAsString();
            const auto resp = juce::JSON::parse (texto);

            juce::MessageManager::callAsync ([eu, resp]
            {
                auto* l = eu.get();
                const auto nova = resp["atualizacao"];
                if (l == nullptr || ! (bool) resp["ok"] || ! nova.isObject()) return;
                const auto v = nova["versao"].toString(), link = nova["url"].toString();
                if (compararVersao (v, versaoAtual()) <= 0 || ! link.startsWithIgnoreCase ("https://")) return;
                l->novaVersao = v;
                l->urlNova = link;
                l->sendChangeMessage();
            });
        });
    }

    void resposta (const juce::String& acao, const juce::String& email, const juce::String& token,
                   const juce::var& resp, bool mostrarErro)
    {
        emAndamento = false;
        const auto f = arquivo();

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
                msg = utf8 ("Ativado. Bom som!");
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
    bool emAndamento = false, jaConfirmou = false, versaoPerguntada = false;
    juce::String msg, emailAtual, tokenAtual, novaVersao, urlNova, dispensada;
    juce::int64 emitidaSeg = 0, validaAteSeg = 0;
    juce::Time ultimaData;
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (Licenca)
    JUCE_DECLARE_NON_COPYABLE (Licenca)
};
} // namespace isp
