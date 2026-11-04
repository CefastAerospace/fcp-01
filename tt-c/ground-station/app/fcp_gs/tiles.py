"""Folhas do mapa (OpenStreetMap) com cache em disco, para uso sem internet.

Com rede, cada folha pedida pela interface é baixada uma vez e guardada em
cache/tiles/z/x/y.png. Sem rede, só o que está no cache aparece; o resto da
área fica com o contorno offline dos países (static/vendor/borders).

Antes de ir ao local de testes, pré-carregue a região com
`python -m fcp_gs tiles --bbox S,W,N,E --zoom 4-11`.
"""

import math
import time
import urllib.error
import urllib.request
from pathlib import Path

OSM_URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
# O OSM exige um contato no User-Agent; sem ele, responde 200 com a imagem
# "Access blocked" e o header X-Blocked.
USER_AGENT = "FCP-01-ground-station/1.0 (+https://github.com/CefastAerospace/fcp-01)"
MAX_ZOOM = 17
FETCH_TIMEOUT_S = 3.0
PREFETCH_LIMIT = 2000      # Política de uso do OSM: nada de downloads em massa.
PREFETCH_PAUSE_S = 0.1


def valid_tile(z: int, x: int, y: int) -> bool:
    return 0 <= z <= MAX_ZOOM and 0 <= x < 2 ** z and 0 <= y < 2 ** z


def lonlat_to_tile(lat: float, lon: float, z: int) -> tuple[int, int]:
    """Coordenada geográfica para o índice da folha (Web Mercator)."""
    lat = max(min(lat, 85.0511), -85.0511)
    n = 2 ** z
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n)
    return min(max(x, 0), n - 1), min(max(y, 0), n - 1)


def tiles_in_bbox(bbox: tuple[float, float, float, float], zmin: int, zmax: int) -> list[tuple[int, int, int]]:
    """Folhas que cobrem bbox = (sul, oeste, norte, leste) de zmin a zmax."""
    south, west, north, east = bbox
    tiles = []
    for z in range(zmin, zmax + 1):
        x0, y0 = lonlat_to_tile(north, west, z)
        x1, y1 = lonlat_to_tile(south, east, z)
        tiles.extend((z, x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1))
    return tiles


class TileCache:
    def __init__(self, cache_dir: Path, upstream: str | None = OSM_URL) -> None:
        self.cache_dir = Path(cache_dir)
        self.upstream = upstream

    def path(self, z: int, x: int, y: int) -> Path:
        return self.cache_dir / str(z) / str(x) / f"{y}.png"

    def get(self, z: int, x: int, y: int) -> bytes | None:
        """Folha do cache; se faltar, baixa e grava. None sem rede ou fora do intervalo."""
        if not valid_tile(z, x, y):
            return None
        path = self.path(z, x, y)
        if path.is_file():
            return path.read_bytes()
        data = self._fetch(z, x, y)
        if data is not None:
            path.parent.mkdir(parents=True, exist_ok=True)
            tmp = path.with_suffix(".tmp")
            tmp.write_bytes(data)
            tmp.replace(path)
        return data

    def _fetch(self, z: int, x: int, y: int) -> bytes | None:
        if not self.upstream:
            return None
        request = urllib.request.Request(self.upstream.format(z=z, x=x, y=y),
                                         headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(request, timeout=FETCH_TIMEOUT_S) as response:
                if response.status != 200 or response.headers.get("X-Blocked"):
                    return None  # Nunca guardar a imagem de bloqueio no cache.
                if response.headers.get_content_type() != "image/png":
                    return None
                return response.read()
        except (urllib.error.URLError, OSError, ValueError):
            return None

    def prefetch(self, bbox: tuple[float, float, float, float], zmin: int, zmax: int,
                 pause_s: float = PREFETCH_PAUSE_S, progress=None) -> dict:
        tiles = tiles_in_bbox(bbox, zmin, zmax)
        if len(tiles) > PREFETCH_LIMIT:
            raise ValueError(f"{len(tiles)} folhas; o limite é {PREFETCH_LIMIT}. "
                             "Reduza a área ou o zoom máximo.")
        result = {"total": len(tiles), "cached": 0, "fetched": 0, "failed": 0}
        for z, x, y in tiles:
            if self.path(z, x, y).is_file():
                result["cached"] += 1
            elif self.get(z, x, y) is not None:
                result["fetched"] += 1
                time.sleep(pause_s)
            else:
                result["failed"] += 1
            if progress:
                progress(result)
        return result
