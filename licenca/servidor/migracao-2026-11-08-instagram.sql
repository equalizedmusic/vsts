-- Robô do Instagram (src/instagram.js): um comentário com palavra-chave é respondido uma vez só.
CREATE TABLE IF NOT EXISTS ig_respostas (
    comentario_id TEXT PRIMARY KEY,
    ig_id        TEXT,
    midia_id     TEXT,
    usuario      TEXT,
    palavra      TEXT,
    texto        TEXT,
    quando       TEXT NOT NULL,
    dm_ok        INTEGER,
    publico_ok   INTEGER,
    detalhe      TEXT
);
CREATE INDEX IF NOT EXISTS ig_respostas_quando ON ig_respostas (quando);
