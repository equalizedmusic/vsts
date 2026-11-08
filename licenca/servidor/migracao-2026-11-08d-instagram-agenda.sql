-- Robô do Instagram (src/instagram.js): agenda de posts (foto, carrossel, Reels) publicada pelo cron de 10 em 10 min.
CREATE TABLE IF NOT EXISTS ig_agenda (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tipo        TEXT NOT NULL,          -- imagem | reels | carrossel
    midias      TEXT NOT NULL,          -- JSON: endereços https
    legenda     TEXT,
    quando      TEXT NOT NULL,          -- ISO UTC
    estado      TEXT NOT NULL,          -- pendente | processando | publicado | erro | cancelado
    conteiner   TEXT,                   -- JSON: ids dos contêineres da Meta
    resultado   TEXT,
    tentativas  INTEGER DEFAULT 0,
    criado      TEXT,
    atualizado  TEXT
);
CREATE INDEX IF NOT EXISTS ig_agenda_estado ON ig_agenda (estado, quando);
