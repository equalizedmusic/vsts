-- Loja: carrinho + cupom (12/11/2026). Carrinho não muda a tabela (pacotes juntos em pedidos.pacote com "+").
-- cupom: código usado (minúsculo, sem "#"); desconto_centavos: quanto saiu do total. Aplicar com:
--   npx wrangler d1 execute isp-licencas --remote --file migracao-2026-11-12-cupom.sql
ALTER TABLE pedidos ADD COLUMN cupom TEXT;
ALTER TABLE pedidos ADD COLUMN desconto_centavos INTEGER NOT NULL DEFAULT 0;
CREATE INDEX IF NOT EXISTS pedidos_cupom ON pedidos (cupom, email);
-- pós-venda (e-mail 3 dias depois do pagamento): quando saiu (-1 = envio falhou)
ALTER TABLE pedidos ADD COLUMN posvenda_em INTEGER;
