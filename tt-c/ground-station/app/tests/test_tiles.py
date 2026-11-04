"""Folhas do mapa: cache local, falta de rede e pré-carga limitada."""

import pytest

from fcp_gs.server import create_app
from fcp_gs.tiles import PREFETCH_LIMIT, TileCache, lonlat_to_tile, tiles_in_bbox

PNG = b"\x89PNG\r\n\x1a\nfolha"
OFFLINE = "http://127.0.0.1:9/{z}/{x}/{y}.png"  # Porta discard: conexão recusada.


def test_cached_tile_is_served_without_network(make_link, tmp_path):
    cache = TileCache(tmp_path / "tiles", upstream=OFFLINE)
    path = cache.path(5, 11, 17)
    path.parent.mkdir(parents=True)
    path.write_bytes(PNG)
    client = create_app(make_link(), tmp_path, tile_cache=cache).test_client()

    response = client.get("/tiles/5/11/17.png")
    assert response.status_code == 200
    assert response.data == PNG
    assert response.mimetype == "image/png"


def test_missing_tile_without_network_is_404(make_link, tmp_path):
    cache = TileCache(tmp_path / "tiles", upstream=OFFLINE)
    client = create_app(make_link(), tmp_path, tile_cache=cache).test_client()

    assert client.get("/tiles/5/11/17.png").status_code == 404
    assert not cache.path(5, 11, 17).exists()


@pytest.mark.parametrize("url", ["/tiles/18/0/0.png", "/tiles/3/8/0.png", "/tiles/3/0/8.png"])
def test_out_of_range_tile_is_404(make_link, tmp_path, url):
    cache = TileCache(tmp_path / "tiles", upstream=None)
    client = create_app(make_link(), tmp_path, tile_cache=cache).test_client()
    assert client.get(url).status_code == 404


def test_fetched_tile_is_cached(tmp_path, monkeypatch):
    cache = TileCache(tmp_path / "tiles")
    monkeypatch.setattr(cache, "_fetch", lambda z, x, y: PNG)
    assert cache.get(4, 5, 8) == PNG
    assert cache.path(4, 5, 8).read_bytes() == PNG


def test_lonlat_to_tile():
    assert lonlat_to_tile(0.0, 0.0, 1) == (1, 1)
    assert lonlat_to_tile(-15.79, -47.88, 9) == (187, 278)


def test_bbox_tiles_and_limit(tmp_path):
    tiles = tiles_in_bbox((-16.2, -48.2, -15.4, -47.3), 4, 6)
    assert (4, 5, 8) in tiles
    assert len(tiles) == len(set(tiles))

    with pytest.raises(ValueError):
        TileCache(tmp_path, upstream=None).prefetch((-60.0, -80.0, 10.0, -30.0), 4, 12)
    assert len(tiles_in_bbox((-60.0, -80.0, 10.0, -30.0), 4, 12)) > PREFETCH_LIMIT


def _serve(headers: dict, body: bytes):
    """Servidor HTTP local que responde toda folha com headers e body fixos."""
    import threading
    from http.server import BaseHTTPRequestHandler, HTTPServer

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            for key, value in headers.items():
                self.send_header(key, value)
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    server = HTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, f"http://127.0.0.1:{server.server_port}/{{z}}/{{x}}/{{y}}.png"


def test_osm_block_image_is_not_cached(tmp_path):
    server, url = _serve({"Content-Type": "image/png", "X-Blocked": "Access denied."}, PNG)
    try:
        cache = TileCache(tmp_path / "tiles", upstream=url)
        assert cache.get(4, 5, 8) is None
        assert not cache.path(4, 5, 8).exists()
    finally:
        server.shutdown()


def test_valid_upstream_tile_is_cached(tmp_path):
    server, url = _serve({"Content-Type": "image/png"}, PNG)
    try:
        cache = TileCache(tmp_path / "tiles", upstream=url)
        assert cache.get(4, 5, 8) == PNG
        assert cache.path(4, 5, 8).read_bytes() == PNG
    finally:
        server.shutdown()
