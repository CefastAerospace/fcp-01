# Contornos offline do mapa

`borders.geojson` junta duas camadas do Natural Earth v5.1.2, escala 1:50m:

- `ne_50m_admin_0_countries`: países;
- `ne_50m_admin_1_states_provinces_lines`: divisas estaduais (inclui o Brasil).

As coordenadas foram arredondadas para 3 casas e as propriedades reduzidas a `k` (`country` ou `state`) e `name`.

Natural Earth é de domínio público (https://www.naturalearthdata.com/about/terms-of-use/).

O arquivo é a camada de fundo do mapa quando não há internet nem folhas OSM em cache.
