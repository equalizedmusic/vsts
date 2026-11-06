-- Loja: nome completo do comprador (vai ao Mercado Pago em payer.name/surname e abre o e-mail da licença). Aplicar com:
--   npx wrangler d1 execute isp-licencas --remote --file migracao-2026-11-06-pedidos-nome.sql
ALTER TABLE pedidos ADD COLUMN nome TEXT;
