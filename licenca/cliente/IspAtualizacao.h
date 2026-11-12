#pragma once

// Atualização automática (só nos plugins compilados com ISP_ATUALIZAR=1: isp_licenca(<alvo> <pacote> ATUALIZAR)).
//
// O servidor (GET /v1/versao) devolve, junto do aviso de versão nova, os dados do instalador ASSINADOS com a mesma
// chave Ed25519 das licenças: { v, tipo:"instalador", pacote, sistema, versao, sha256, tamanho, url }. A Licenca
// confere a assinatura e passa para cá. ATUALIZAR baixa o instalador para a pasta temporária, confere o SHA-256 e:
//   Windows: deixa um script esperando o DAW fechar (o .vst3 fica travado enquanto ele está aberto); quando fecha,
//            roda o instalador em silêncio (o Windows pede permissão de administrador uma vez). Abriu o DAW de novo,
//            é a versão nova. Log em %TEMP%\InsideStudioSP-atualizacao\<pacote>-<versao>\*.log
//   Mac:     monta o .dmg e roda o .pkg com `installer` (o Mac pede a senha de administrador); depois é só fechar e
//            abrir o DAW. Sem senha / cancelou: avisa e o BAIXAR continua valendo.
// Qualquer falha cai no caminho antigo (BAIXAR abre o instalador no navegador).

#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
 #include <process.h>
#endif

namespace isp
{
class Atualizador : public juce::ChangeBroadcaster, private juce::Thread, private juce::AsyncUpdater
{
public:
    enum class Estado { parado, baixando, conferindo, instalando, esperaFechar, pronto, erro };

    Atualizador() : juce::Thread ("isp-atualizar") {}
    ~Atualizador() override
    {
        stopThread (8000);
        cancelPendingUpdate();
    }

    // dados já conferidos pela Licenca (assinatura ok, pacote/sistema certos)
    void preparar (const juce::String& pacote, const juce::String& nomeVisivel, const juce::String& versao,
                   const juce::String& url, const juce::String& sha256, juce::int64 tamanho)
    {
        if (isThreadRunning()) return;
        const juce::ScopedLock sl (lock);
        pac = pacote; nome = nomeVisivel; ver = versao; link = url; sha = sha256.toLowerCase(); total = tamanho;
    }

    bool disponivel() const { const juce::ScopedLock sl (lock); return link.isNotEmpty(); }
    juce::String versao() const { const juce::ScopedLock sl (lock); return ver; }
    Estado estado() const { const juce::ScopedLock sl (lock); return est; }
    juce::String mensagem() const { const juce::ScopedLock sl (lock); return msg; }
    float progresso() const noexcept { return prog.load(); }

    void iniciar()
    {
        if (isThreadRunning() || ! disponivel()) return;
        if (const auto e = estado(); e != Estado::parado && e != Estado::erro) return;
        prog = 0.0f;
        mudar (Estado::baixando);
        startThread();
    }

private:
    static juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

    void mudar (Estado e, const juce::String& m = {})
    {
        {
            const juce::ScopedLock sl (lock);
            est = e;
            msg = m;
        }
        triggerAsyncUpdate();
    }

    void falhou (const char* texto) { mudar (Estado::erro, utf8 (texto)); }

    void handleAsyncUpdate() override { sendChangeMessage(); }

    void run() override
    {
        juce::String url, versaoNova, hash, pacote, nomeVisivel;
        juce::int64 tamanho = 0;
        {
            const juce::ScopedLock sl (lock);
            url = link; versaoNova = ver; hash = sha; pacote = pac; nomeVisivel = nome; tamanho = total;
        }

        const auto pasta = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("InsideStudioSP-atualizacao")
                               .getChildFile (pacote + "-" + versaoNova);
        pasta.deleteRecursively();
        if (! pasta.createDirectory())
            return falhou ("Não consegui criar a pasta temporária. Use BAIXAR.");

       #if JUCE_WINDOWS
        const auto arquivo = pasta.getChildFile ("instalador.exe");
       #else
        const auto arquivo = pasta.getChildFile ("instalador.dmg");
       #endif

        // ---- baixar ----
        {
            int status = 0;
            auto opcoes = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs (20000)
                              .withNumRedirectsToFollow (8)
                              .withStatusCode (&status);
            auto in = juce::URL (url).createInputStream (opcoes);
            if (in == nullptr || status >= 400)
                return falhou ("Não consegui baixar a versão nova. Confira a internet e tente de novo, ou use BAIXAR.");

            juce::FileOutputStream out (arquivo);
            if (out.failedToOpen())
                return falhou ("Não consegui gravar o arquivo na pasta temporária. Use BAIXAR.");

            const auto esperado = tamanho > 0 ? tamanho : in->getTotalLength();
            juce::HeapBlock<char> buf (1 << 16);
            juce::int64 lido = 0;
            auto ultimoAviso = juce::Time::getMillisecondCounter();
            while (! threadShouldExit())
            {
                const auto n = in->read (buf.get(), 1 << 16);
                if (n <= 0) break;
                if (! out.write (buf.get(), (size_t) n))
                    return falhou ("Disco cheio ao baixar a versão nova.");
                lido += n;
                if (esperado > 0) prog = (float) juce::jlimit (0.0, 1.0, (double) lido / (double) esperado);
                if (juce::Time::getMillisecondCounter() - ultimoAviso > 150)
                {
                    ultimoAviso = juce::Time::getMillisecondCounter();
                    triggerAsyncUpdate();
                }
            }
            out.flush();
            if (threadShouldExit()) return;
            if (tamanho > 0 && lido != tamanho)
                return falhou ("O download parou no meio. Tente de novo, ou use BAIXAR.");
        }

        // ---- conferir: o SHA-256 veio assinado pelo servidor ----
        mudar (Estado::conferindo);
        if (juce::SHA256 (arquivo).toHexString().removeCharacters (" ").toLowerCase() != hash)
        {
            arquivo.deleteFile();
            return falhou ("O arquivo baixado não conferiu (pode ter vindo corrompido). Nada foi instalado. Use BAIXAR.");
        }
        if (threadShouldExit()) return;

       #if JUCE_WINDOWS
        // ---- Windows: script espera o DAW (este processo) fechar e roda o instalador em silêncio ----
        const auto script = pasta.getChildFile ("atualizar.ps1");
        const char* ps =
            "param([string]$Exe, [int]$DawPid)\r\n"
            "$log = [IO.Path]::ChangeExtension($Exe, '.log')\r\n"
            "try { Wait-Process -Id $DawPid -ErrorAction SilentlyContinue } catch {}\r\n"
            "Start-Sleep -Seconds 2\r\n"
            "try {\r\n"
            "  $p = Start-Process -FilePath $Exe -Verb RunAs -Wait -PassThru -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES',"
            "'/NORESTART','/NOCLOSEAPPLICATIONS',('/LOG=\"' + $log + '\"'))\r\n"
            "  ('fim ' + $p.ExitCode) | Out-File -Encoding ascii ($log + '.status')\r\n"
            "} catch { ('erro ' + $_.Exception.Message) | Out-File -Encoding ascii ($log + '.status') }\r\n";
        if (! script.replaceWithText (ps, false, false, "\r\n"))
            return falhou ("Não consegui preparar a instalação. Use BAIXAR.");

        juce::ChildProcess proc;   // no Windows o destrutor não mata o processo filho: o script continua sozinho
        const juce::StringArray args { "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                       "-WindowStyle", "Hidden", "-File", script.getFullPathName(),
                                       "-Exe", arquivo.getFullPathName(), "-DawPid", juce::String ((int) _getpid()) };
        if (! proc.start (args, 0))
            return falhou ("Não consegui preparar a instalação. Use BAIXAR.");
        mudar (Estado::esperaFechar);
       #else
        // ---- Mac: monta o dmg e roda o pkg como administrador (o Mac pede a senha) ----
        mudar (Estado::instalando);
        const juce::String comando =
            "on run argv\n"
            "do shell script \"M=$(/usr/bin/mktemp -d /tmp/ispupd.XXXXXX) && "
            "/usr/bin/hdiutil attach -nobrowse -noverify -noautoopen -quiet -mountpoint \\\"$M\\\" \" & quoted form of (item 1 of argv) & \" && "
            "P=$(/bin/ls \\\"$M\\\"/*.pkg | /usr/bin/head -n 1) && /usr/sbin/installer -pkg \\\"$P\\\" -target /; "
            "R=$?; /usr/bin/hdiutil detach -quiet \\\"$M\\\" 2>/dev/null; exit $R\" "
            "with prompt (item 2 of argv) with administrator privileges\n"
            "end run";
        const auto pergunta = utf8 ("InsideStudioSP: instalar a versão ") + versaoNova + " do " + nomeVisivel + ".";
        juce::ChildProcess proc;
        if (! proc.start (juce::StringArray { "/usr/bin/osascript", "-e", comando, arquivo.getFullPathName(), pergunta }, 0))
            return falhou ("Não consegui iniciar o instalador. Use BAIXAR.");
        while (proc.isRunning())
        {
            if (threadShouldExit()) { proc.kill(); return; }
            wait (250);
        }
        if (proc.getExitCode() != 0)
            return falhou ("A instalação não terminou (senha cancelada ou erro). Tente de novo, ou use BAIXAR.");
        arquivo.deleteFile();
        mudar (Estado::pronto);
       #endif
    }

    juce::CriticalSection lock;
    juce::String pac, nome, ver, link, sha, msg;
    juce::int64 total = 0;
    Estado est = Estado::parado;
    std::atomic<float> prog { 0.0f };

    JUCE_DECLARE_NON_COPYABLE (Atualizador)
};
} // namespace isp
