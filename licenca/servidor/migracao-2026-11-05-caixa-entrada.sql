-- 05/11/2026: caixa de entrada do contato@insidestudiosp.com.br no painel (tabela nova, não mexe nas outras).
-- Aplicar uma vez, ANTES de publicar o Worker com a caixa (sem a tabela o envio segue funcionando, só não registra):
--   npx wrangler d1 execute isp-licencas --remote --file migracao-2026-11-05-caixa-entrada.sql
CREATE TABLE IF NOT EXISTS mensagens (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    direcao    TEXT    NOT NULL,          -- entrada | saida
    resend_id  TEXT,                      -- id do e-mail no Resend
    contato    TEXT    NOT NULL,          -- o outro lado da conversa (e-mail minúsculo)
    de         TEXT,
    para       TEXT,
    assunto    TEXT,
    texto      TEXT,                      -- só texto puro; HTML recebido vira texto (o painel nunca renderiza HTML)
    message_id TEXT,                      -- Message-ID do recebido (para a resposta cair na mesma conversa)
    anexos     TEXT,                      -- JSON [{ id, nome, tipo, tamanho }] (o arquivo fica no Resend)
    origem     TEXT,                      -- recebido | automatico (cadastro/token) | painel
    lida       INTEGER NOT NULL DEFAULT 0,
    quando     INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS mensagens_resend ON mensagens (direcao, resend_id);
CREATE INDEX IF NOT EXISTS mensagens_contato ON mensagens (contato, id);
