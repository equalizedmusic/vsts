-- Robô do Instagram: guarda os últimos avisos crus da Meta (diagnóstico: saber se o webhook está chegando)
CREATE TABLE IF NOT EXISTS ig_eventos (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    quando TEXT NOT NULL,
    corpo TEXT
);
