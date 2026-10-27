-- 27/10/2026: um token por e-mail (vale para todos os pacotes daquele e-mail) + login do painel.
-- O token_hash deixa de ser UNIQUE (várias licenças do mesmo e-mail guardam o mesmo hash). SQLite não remove
-- restrição de coluna, então a tabela é recriada. Aplicar uma vez só:
--   npx wrangler d1 execute isp-licencas --remote --file migracao-2026-10-27-token-por-email.sql

-- as ativações apontam para licencas(id): guardo, esvazio e devolvo no fim (o D1 recusa o DROP com ativação viva)
CREATE TABLE ativacoes_copia AS SELECT * FROM ativacoes;
DELETE FROM ativacoes;

CREATE TABLE licencas_nova (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    email        TEXT    NOT NULL,
    pacote       TEXT    NOT NULL,
    token_hash   TEXT    NOT NULL,
    max_maquinas INTEGER NOT NULL DEFAULT 2,
    status       TEXT    NOT NULL DEFAULT 'ativa',
    origem       TEXT    NOT NULL DEFAULT 'cortesia',
    pedido       TEXT,
    nota         TEXT,
    criada_em    INTEGER NOT NULL,
    revogada_em  INTEGER,
    UNIQUE (email, pacote)
);
INSERT INTO licencas_nova (id, email, pacote, token_hash, max_maquinas, status, origem, pedido, nota, criada_em, revogada_em)
    SELECT id, email, pacote, token_hash, max_maquinas, status, origem, pedido, nota, criada_em, revogada_em FROM licencas;
DROP TABLE licencas;
ALTER TABLE licencas_nova RENAME TO licencas;
CREATE INDEX IF NOT EXISTS licencas_token ON licencas (token_hash);

INSERT INTO ativacoes (id, licenca_id, maquina, nome_maquina, criada_em, visto_em)
    SELECT id, licenca_id, maquina, nome_maquina, criada_em, visto_em FROM ativacoes_copia;
DROP TABLE ativacoes_copia;

CREATE TABLE IF NOT EXISTS painel (
    usuario    TEXT    PRIMARY KEY,
    senha_hash TEXT    NOT NULL,
    sal        TEXT    NOT NULL,
    iteracoes  INTEGER NOT NULL,
    trocada_em INTEGER NOT NULL
);
