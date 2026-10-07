#!/usr/bin/env python3
"""Rebuild a real ETOPO Alps window from a small, pinned source-byte bundle.

Default operation is fully offline. --fetch retrieves only two fixed ranges of
the original NOAA TIFF and the pinned official license metadata. This sample
does not provide a complete global dataset or building-scale survey accuracy.
Python 3.9+, standard library only.
"""
import argparse
import base64
import hashlib
import json
import math
import struct
import urllib.request
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUNDLE_PATH = ROOT / "data/geo/sources/etopo2022_n60e000_tile201.json"
METADATA_PATH = ROOT / "data/geo/sources/etopo2022_noaa_metadata.xml"
SAMPLE_PATH = ROOT / "data/geo/etopo2022_alps_sample.json"
TIFF_URL = ("https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/15s/"
            "15s_surface_elev_gtif/ETOPO_2022_v1_15s_N60E000_surface.tif")
METADATA_URL = ("https://www.ncei.noaa.gov/metadata/geoportal/rest/metadata/item/"
                "gov.noaa.ngdc.mgg.dem%3Aetopo_2022/xml")
SOURCE_TOTAL_BYTES = 33233231
METADATA_BYTES = 163048
METADATA_SHA256 = "8324c8d3cc926ade7ae13d950b847cdd2b46fc6aac2aa057fe6391597f7399e0"
RANGES = (
    ("tiff_header", 0, 2517,
     "f74b1488e3d2a2dd7e5ce6fa6c7a30934d61ccfc5263aa371c8a3e3f93538de4"),
    ("tile_201", 31626565, 196730,
     "851b9d0a47a90467740866a878f94fb51b072bda5f7ce814e9b6305c48c1962e"),
)
TILE_INDEX = 201
TILE_X, TILE_Y, TILE_EDGE = 1536, 3328, 256
WINDOW_X, WINDOW_Y, WINDOW_EDGE = 1668, 3372, 25


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False)
            + "\n").encode("utf-8")


def relative(path):
    return path.resolve().relative_to(ROOT).as_posix()


def metadata_provenance():
    return {"path": relative(METADATA_PATH), "url": METADATA_URL,
            "sha256": METADATA_SHA256, "byte_count": METADATA_BYTES,
            "role": "official_NOAA_CC0_waiver_and_dataset_metadata"}


def make_bundle(chunks):
    return {
        "schema": "sonnheide.etopo.source_ranges.v1",
        "dataset": "NOAA ETOPO 2022 v1, 15 arc-second Ice Surface",
        "source_url": TIFF_URL,
        "source_total_bytes": SOURCE_TOTAL_BYTES,
        "partial_source": True,
        "full_file_sha256": None,
        "scope": "Exact original TIFF header and tile 201 only; global package not downloaded",
        "license": "CC0-1.0",
        "license_evidence": metadata_provenance(),
        "ranges": [{"id": name, "offset": offset, "byte_count": count,
                    "sha256": digest, "encoding": "base64",
                    "data": base64.b64encode(chunk).decode("ascii")}
                   for (name, offset, count, digest), chunk in zip(RANGES, chunks)],
    }


def validate_metadata(raw):
    require(len(raw) == METADATA_BYTES and sha256(raw) == METADATA_SHA256,
            "Official NOAA metadata does not match the pinned original bytes")
    # Parse as data only. ElementTree does not fetch external entities or URLs.
    require(b"<!DOCTYPE" not in raw and b"<!ENTITY" not in raw,
            "External entities are outside the pinned metadata contract")
    text = " ".join(" ".join(ET.fromstring(raw).itertext()).split())
    require("NOAA waives any potential copyright and related rights" in text
            and "CC0-1.0" in text and "etopo_2022" in raw.decode("utf-8"),
            "NOAA metadata must include this dataset and its worldwide CC0 waiver")


def decode_bundle(raw):
    require(len(raw) < 400000, "Unexpected source bundle size")
    bundle = json.loads(raw.decode("utf-8"))
    require(isinstance(bundle, dict), "Source bundle must be an object")
    require(bundle.get("schema") == "sonnheide.etopo.source_ranges.v1"
            and bundle.get("source_url") == TIFF_URL
            and bundle.get("source_total_bytes") == SOURCE_TOTAL_BYTES
            and bundle.get("partial_source") is True
            and "full_file_sha256" in bundle
            and bundle["full_file_sha256"] is None
            and bundle.get("license") == "CC0-1.0"
            and bundle.get("license_evidence") == metadata_provenance(),
            "Source bundle identity, scope or license evidence changed")
    entries = bundle.get("ranges")
    require(isinstance(entries, list) and len(entries) == len(RANGES),
            "Exactly the pinned TIFF header and tile ranges are required")
    chunks = []
    for entry, (name, offset, count, digest) in zip(entries, RANGES):
        require(isinstance(entry, dict) and entry.get("id") == name
                and entry.get("offset") == offset and entry.get("byte_count") == count
                and entry.get("sha256") == digest and entry.get("encoding") == "base64"
                and isinstance(entry.get("data"), str), "Source byte-range identity changed")
        chunk = base64.b64decode(entry["data"], validate=True)
        require(len(chunk) == count and sha256(chunk) == digest,
                "Original source byte-range checksum mismatch: " + name)
        chunks.append(chunk)
    require(raw == json_bytes(make_bundle(chunks)),
            "Bundle must have the deterministic canonical representation")
    return chunks


def parse_tiff_header(header):
    require(header[:4] == b"II\x2a\x00", "Expected classic little-endian TIFF")
    ifd = struct.unpack_from("<I", header, 4)[0]
    require(ifd == 8, "Unexpected TIFF IFD offset")
    entries = struct.unpack_from("<H", header, ifd)[0]
    require(entries == 22 and ifd + 2 + 12 * entries + 4 <= len(header),
            "Unexpected or truncated TIFF directory")
    tags = {}
    sizes = {2: 1, 3: 2, 4: 4, 12: 8}
    codes = {3: "H", 4: "I", 12: "d"}
    for i in range(entries):
        pos = ifd + 2 + 12 * i
        tag, kind, count, value = struct.unpack_from("<HHII", header, pos)
        require(tag not in tags and kind in sizes and 0 < count <= 225,
                "Unsupported or repeated TIFF tag")
        size = count * sizes[kind]
        if size <= 4:
            payload = header[pos + 8:pos + 8 + size]
        else:
            require(value + size <= len(header), "TIFF tag escapes the retained header")
            payload = header[value:value + size]
        if kind == 2:
            require(payload.endswith(b"\x00"), "TIFF ASCII value is not terminated")
            tags[tag] = payload[:-1].decode("utf-8")
        else:
            tags[tag] = list(struct.unpack("<" + codes[kind] * count, payload))
    require(struct.unpack_from("<I", header, ifd + 2 + 12 * entries)[0] == 0,
            "Multiple TIFF directories are outside this sample contract")
    expected = {
        256: [3600], 257: [3600], 258: [32], 259: [8], 262: [1],
        277: [1], 284: [1], 317: [3], 322: [256], 323: [256], 339: [3],
        33550: [1.0 / 240, 1.0 / 240, 1.0],
        33922: [0.0, 0.0, 0.0, 0.0, 60.0, 0.0],
        34735: [1, 1, 1, 5, 1024, 0, 1, 2, 1025, 0, 1, 1,
                1026, 34737, 24, 0, 2048, 0, 1, 4326, 4096, 0, 1, 3855],
        34737: "WGS 84 + EGM2008 height|", 42113: "-99999",
    }
    require(all(tags.get(tag) == expected_value
                for tag, expected_value in expected.items()),
            "TIFF raster type, encoding, dimensions, NoData or georeferencing changed")
    offsets, counts = tags.get(324), tags.get(325)
    require(isinstance(offsets, list) and isinstance(counts, list)
            and len(offsets) == len(counts) == 225,
            "Expected exactly 225 TIFF tile offsets and lengths")
    end = len(header)
    for offset, count in zip(offsets, counts):
        require(offset == end and count > 0 and offset + count <= SOURCE_TOTAL_BYTES,
                "TIFF tile ranges must be consecutive and within the original source")
        end = offset + count
    require(end == SOURCE_TOTAL_BYTES and offsets[TILE_INDEX] == RANGES[1][1]
            and counts[TILE_INDEX] == RANGES[1][2], "Pinned tile range disagrees with TIFF IFD")
    return tags


def decode_tile(compressed):
    expected_bytes = TILE_EDGE * TILE_EDGE * 4
    inflater = zlib.decompressobj()
    raw = inflater.decompress(compressed, expected_bytes + 1)
    require(len(raw) == expected_bytes and inflater.eof and not inflater.unused_data
            and not inflater.unconsumed_tail, "Deflate tile length or stream boundary is invalid")
    values = []
    for y in range(TILE_EDGE):
        row = bytearray(raw[y * TILE_EDGE * 4:(y + 1) * TILE_EDGE * 4])
        # Predictor 3 differences the byte-shuffled row continuously, including
        # transitions between its four significance planes; stride is one.
        for i in range(1, len(row)):
            row[i] = (row[i] + row[i - 1]) & 255
        restored = bytearray(TILE_EDGE * 4)
        for x in range(TILE_EDGE):
            for plane in range(4):
                restored[4 * x + 3 - plane] = row[plane * TILE_EDGE + x]
        values.extend(struct.unpack("<256f", restored))
    require(all(math.isfinite(value) and value != -99999 and -11000 <= value <= 9000
                for value in values), "Source tile has missing or implausible elevations")
    # Independent pinned source facts catch a wrong predictor/byte-order decoder.
    require(values[(3384 - TILE_Y) * TILE_EDGE + 1680 - TILE_X] == 3496.6357421875
            and min(values) == 325.9329833984375 and max(values) == 4692.80859375,
            "Decoded source values do not match the pinned Alps tile")
    return values


def make_sample(bundle_raw, metadata_raw):
    validate_metadata(metadata_raw)
    header, compressed = decode_bundle(bundle_raw)
    parse_tiff_header(header)
    tile = decode_tile(compressed)
    rows = [[tile[(y - TILE_Y) * TILE_EDGE + x - TILE_X]
             for x in range(WINDOW_X, WINDOW_X + WINDOW_EDGE)]
            for y in range(WINDOW_Y, WINDOW_Y + WINDOW_EDGE)]
    return {
        "schema": "sonnheide.terrain.geographic_sample.v1",
        "status": "verified_real_source_window_not_global_world_import",
        "dataset": "NOAA ETOPO 2022 v1, 15 arc-second Ice Surface",
        "provenance": {"source_bundle": relative(BUNDLE_PATH),
                       "source_bundle_sha256": sha256(bundle_raw),
                       "source_url": TIFF_URL, "source_total_bytes": SOURCE_TOTAL_BYTES,
                       "partial_source": True, "full_file_sha256": None,
                       "retained_source_bytes": sum(r[2] for r in RANGES),
                       "license": "CC0-1.0", "license_evidence": metadata_provenance(),
                       "citation": "NOAA National Centers for Environmental Information. 2022: "
                                   "ETOPO 2022 15 Arc-Second Global Relief Model. "
                                   "https://doi.org/10.25921/fd45-gt74",
                       "importer": relative(Path(__file__)),
                       "source_decode": "TIFF little-endian float32, Adobe Deflate, Predictor 3"},
        "scope": "25 by 25 original pixel centres near 45.9 N, 7.0 E; "
                 "global source package not downloaded",
        "crs": {"horizontal_epsg": 4326, "horizontal_datum": "WGS84",
                "vertical_epsg": 3855, "vertical_datum": "EGM2008 geoid",
                "height_unit": "metre", "height_type": "orthometric_height",
                "source_surface": "Ice Surface bare-earth topography/bathymetry model"},
        "sampling": {"source_data_type": "float32", "output_data_type": "JSON numeric exact float32 values",
                     "raster_type": "RasterPixelIsArea", "sample_location": "pixel_centre",
                     "source_nodata": -99999, "missing_values_in_window": 0,
                     "width": WINDOW_EDGE, "height": WINDOW_EDGE,
                     "source_pixel_origin": [WINDOW_X, WINDOW_Y],
                     "longitude_first_centre": (WINDOW_X + 0.5) / 240,
                     "latitude_first_centre": 60 - (WINDOW_Y + 0.5) / 240,
                     "longitude_step_degrees": 1.0 / 240,
                     "latitude_step_degrees": -1.0 / 240,
                     "native_angular_spacing_arcseconds": 15,
                     "native_north_south_spacing_metres_approx": 463,
                     "row_order": "north_to_south", "column_order": "west_to_east",
                     "resampling": "none; original source pixel centres"},
        "height_min_metres": min(min(row) for row in rows),
        "height_max_metres": max(max(row) for row in rows),
        "height_rows_metres": rows,
        "limitations": [
            "This is a retained window of a global dataset, not a complete global source package.",
            "15 arc-second source spacing is insufficient for surveyed building or driveway details.",
            "No coastline or LAND/WATER authority is inferred from height sign; GSHHG remains separate.",
            "Do not treat angular spacing as a constant metre spacing or hide vertical exaggeration.",
            "No projected game-world mesh, foundations or access routes are supplied by this sample.",
        ],
    }


def fetch_fixed(url, count, byte_range=None):
    headers = {"Accept-Encoding": "identity", "User-Agent": "Sonnheide-pinned-ETOPO-import/1"}
    if byte_range is not None:
        offset, length = byte_range
        headers["Range"] = "bytes=%d-%d" % (offset, offset + length - 1)
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        require(response.geturl() == url, "Pinned source unexpectedly redirected")
        require(response.status == (206 if byte_range is not None else 200),
                "Server must return the expected exact range response")
        require(response.headers.get("Content-Encoding", "identity") == "identity",
                "Source must be fetched without transfer content encoding")
        if byte_range is not None:
            expected = "bytes %d-%d/%d" % (offset, offset + length - 1, SOURCE_TOTAL_BYTES)
            require(response.headers.get("Content-Range") == expected,
                    "Server returned a different source range or file length")
        raw = response.read(count + 1)
    require(len(raw) == count, "Pinned download length changed")
    return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true",
                        help="Fetch only pinned original NOAA byte ranges and metadata; validate before writing")
    parser.add_argument("--verify", action="store_true",
                        help="Compare deterministic rebuilt files without writing")
    args = parser.parse_args()
    if args.fetch:
        chunks = []
        for name, offset, count, digest in RANGES:
            chunk = fetch_fixed(TIFF_URL, count, (offset, count))
            require(sha256(chunk) == digest, "Upstream pinned bytes changed: " + name)
            chunks.append(chunk)
        metadata_raw = fetch_fixed(METADATA_URL, METADATA_BYTES)
        bundle_raw = json_bytes(make_bundle(chunks))
    else:
        bundle_raw = BUNDLE_PATH.read_bytes()
        metadata_raw = METADATA_PATH.read_bytes()
    sample_raw = json_bytes(make_sample(bundle_raw, metadata_raw))
    if args.verify:
        require(BUNDLE_PATH.read_bytes() == bundle_raw
                and METADATA_PATH.read_bytes() == metadata_raw
                and SAMPLE_PATH.read_bytes() == sample_raw,
                "Retained source or generated Alps sample differs from its deterministic rebuild")
    else:
        if args.fetch:
            # Nothing is written until both hashes, all TIFF data and license
            # evidence have been validated and the sample has been rebuilt.
            BUNDLE_PATH.parent.mkdir(parents=True, exist_ok=True)
            BUNDLE_PATH.write_bytes(bundle_raw)
            METADATA_PATH.write_bytes(metadata_raw)
        SAMPLE_PATH.parent.mkdir(parents=True, exist_ok=True)
        SAMPLE_PATH.write_bytes(sample_raw)
    sample = json.loads(sample_raw)
    print("PASS: real ETOPO Alps 25x25 window, %.6f..%.6f m; %d retained source bytes; "
          "partial source, global package not downloaded" %
          (sample["height_min_metres"], sample["height_max_metres"], sum(r[2] for r in RANGES)))


if __name__ == "__main__":
    main()
