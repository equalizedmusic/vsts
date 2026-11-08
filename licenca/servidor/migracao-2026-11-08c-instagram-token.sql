-- Robô do Instagram: token do "login do Instagram" (IGAA..., vale 60 dias) renovado pelo cron diário.
-- O segredo IG_TOKEN é só a semente; o token renovado fica aqui e tem prioridade.
CREATE TABLE IF NOT EXISTS ig_config (
    chave TEXT PRIMARY KEY,
    valor TEXT NOT NULL,
    quando TEXT NOT NULL
);
