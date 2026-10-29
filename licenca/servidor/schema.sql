-- Banco das licenças (Cloudflare D1). Aplicar com:
--   npx wrangler d1 execute isp-licencas --remote --file schema.sql

-- uma licença = um e-mail + um pacote. O token é um só por e-mail (todas as licenças do e-mail guardam o mesmo
-- hash); o token não fica gravado, só o SHA-256 dele.
CREATE TABLE IF NOT EXISTS licencas (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    email        TEXT    NOT NULL,                    -- sempre minúsculo, sem espaços
    pacote       TEXT    NOT NULL,                    -- criativo, mix, knob, quimera, limao, mercurio, resenha, montamonta, groovador
    token_hash   TEXT    NOT NULL,
    max_maquinas INTEGER NOT NULL DEFAULT 2,
    status       TEXT    NOT NULL DEFAULT 'ativa',    -- ativa | revogada
    origem       TEXT    NOT NULL DEFAULT 'cortesia', -- cortesia (teste grátis) | venda
    pedido       TEXT,                                -- id do pedido na loja (quando vier da venda)
    nota         TEXT,
    criada_em    INTEGER NOT NULL,
    revogada_em  INTEGER,
    UNIQUE (email, pacote)
);

-- máquinas ativadas de cada licença (até max_maquinas)
CREATE TABLE IF NOT EXISTS ativacoes (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    licenca_id   INTEGER NOT NULL REFERENCES licencas (id),
    maquina      TEXT    NOT NULL,                    -- hash do id da máquina (o plugin manda já em hash)
    nome_maquina TEXT,
    criada_em    INTEGER NOT NULL,
    visto_em     INTEGER NOT NULL,
    UNIQUE (licenca_id, maquina)
);

-- histórico: ativou, validou, desativou, recusou (com o motivo), criou, revogou
CREATE TABLE IF NOT EXISTS eventos (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    licenca_id INTEGER,
    tipo       TEXT    NOT NULL,
    detalhe    TEXT,
    ip         TEXT,
    quando     INTEGER NOT NULL
);

-- login do painel (/painel): senha em PBKDF2-SHA256
CREATE TABLE IF NOT EXISTS painel (
    usuario    TEXT    PRIMARY KEY,
    senha_hash TEXT    NOT NULL,
    sal        TEXT    NOT NULL,
    iteracoes  INTEGER NOT NULL,
    trocada_em INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS licencas_token ON licencas (token_hash);
CREATE INDEX IF NOT EXISTS eventos_licenca ON eventos (licenca_id);
CREATE INDEX IF NOT EXISTS eventos_ip_quando ON eventos (ip, quando);
