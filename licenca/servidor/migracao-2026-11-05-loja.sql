-- Loja (src/loja.js): um pedido por clique em "pagar". Aplicar com:
--   npx wrangler d1 execute isp-licencas --remote --file migracao-2026-11-05-loja.sql
CREATE TABLE IF NOT EXISTS pedidos (
    id             TEXT    PRIMARY KEY,               -- P + 24 hex (vai no external_reference / custom_id)
    pacote         TEXT    NOT NULL,
    email          TEXT    NOT NULL,
    metodo         TEXT    NOT NULL,                  -- mp | paypal
    valor_centavos INTEGER NOT NULL,
    moeda          TEXT    NOT NULL,                  -- BRL | USD
    status         TEXT    NOT NULL DEFAULT 'criado', -- criado | pendente | recusado | processando | pago | erro
    ref            TEXT,                              -- id da preferência (MP) ou do pedido (PayPal)
    pagamento_id   TEXT,                              -- id do pagamento (MP) ou da captura (PayPal)
    forma          TEXT,                              -- Pix, cartão em 3x, boleto, PayPal
    licenca_id     INTEGER,
    email_enviado  INTEGER NOT NULL DEFAULT 0,
    ip             TEXT,
    criado_em      INTEGER NOT NULL,
    pago_em        INTEGER
);
CREATE INDEX IF NOT EXISTS pedidos_ip ON pedidos (ip, criado_em);
