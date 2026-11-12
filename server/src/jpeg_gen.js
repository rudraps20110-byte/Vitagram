/**
 * Pure-JS Baseline DCT JPEG Encoder (ITU-T.81 / JFIF SOF0).
 * Used to generate valid baseline JPEGs for Demo Mode and fallback images
 * without requiring native dependencies.
 */

const ZIGZAG = new Uint8Array([
  0, 1, 5, 6, 14, 15, 27, 28,
  2, 4, 7, 13, 16, 26, 29, 42,
  3, 8, 12, 17, 25, 30, 41, 43,
  9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63,
]);

const STD_LUMA_QT = [
  16, 11, 10, 16, 24, 40, 51, 61,
  12, 12, 14, 19, 26, 58, 60, 55,
  14, 13, 16, 24, 40, 57, 69, 56,
  14, 17, 22, 29, 51, 87, 80, 62,
  18, 22, 37, 56, 68, 109, 103, 77,
  24, 35, 55, 64, 81, 104, 113, 92,
  49, 64, 78, 87, 103, 121, 120, 101,
  72, 92, 95, 98, 112, 100, 103, 99,
];

const STD_CHROMA_QT = [
  17, 18, 24, 47, 99, 99, 99, 99,
  18, 21, 26, 66, 99, 99, 99, 99,
  24, 26, 56, 99, 99, 99, 99, 99,
  47, 66, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
];

const STD_DC_LUMA_NR = [0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0];
const STD_DC_LUMA_VAL = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

const STD_AC_LUMA_NR = [0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d];
const STD_AC_LUMA_VAL = [
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa,
];

const STD_DC_CHROMA_NR = [0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0];
const STD_DC_CHROMA_VAL = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

const STD_AC_CHROMA_NR = [0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77];
const STD_AC_CHROMA_VAL = [
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa,
];

function computeHuffmanTbl(nrCodes, values) {
  const table = [];
  let code = 0;
  let pos = 0;
  for (let len = 1; len <= 16; len++) {
    for (let j = 0; j < nrCodes[len]; j++) {
      table[values[pos++]] = [code, len];
      code++;
    }
    code <<= 1;
  }
  return table;
}

const YDC_HT = computeHuffmanTbl(STD_DC_LUMA_NR, STD_DC_LUMA_VAL);
const UVDC_HT = computeHuffmanTbl(STD_DC_CHROMA_NR, STD_DC_CHROMA_VAL);
const YAC_HT = computeHuffmanTbl(STD_AC_LUMA_NR, STD_AC_LUMA_VAL);
const UVAC_HT = computeHuffmanTbl(STD_AC_CHROMA_NR, STD_AC_CHROMA_VAL);

const BITCODE = new Array(65535);
const CATEGORY = new Array(65535);
{
  let flip = 1;
  let temp = 1;
  for (let cat = 1; cat <= 15; cat++) {
    for (let nr = temp; nr < (1 << cat); nr++) {
      CATEGORY[32767 + nr] = cat;
      BITCODE[32767 + nr] = [nr, cat];
      CATEGORY[32767 - nr] = cat;
      BITCODE[32767 - nr] = [(1 << cat) - 1 - nr, cat];
    }
    flip <<= 1;
    temp <<= 1;
  }
}

function initQuantTables(quality) {
  const sf = quality < 50 ? Math.floor(5000 / quality) : 200 - quality * 2;
  const yTable = new Uint8Array(64);
  const uvTable = new Uint8Array(64);
  const fdtblY = new Float64Array(64);
  const fdtblUV = new Float64Array(64);
  const aasf = [1.0, 1.387039845, 1.306562965, 1.175875602, 1.0, 0.785694958, 0.541196100, 0.275899379];

  for (let i = 0; i < 64; i++) {
    let tY = Math.floor((STD_LUMA_QT[i] * sf + 50) / 100);
    if (tY < 1) tY = 1;
    if (tY > 255) tY = 255;
    yTable[ZIGZAG[i]] = tY;

    let tUV = Math.floor((STD_CHROMA_QT[i] * sf + 50) / 100);
    if (tUV < 1) tUV = 1;
    if (tUV > 255) tUV = 255;
    uvTable[ZIGZAG[i]] = tUV;
  }

  let k = 0;
  for (let row = 0; row < 8; row++) {
    for (let col = 0; col < 8; col++) {
      fdtblY[k] = 1.0 / (yTable[ZIGZAG[k]] * aasf[row] * aasf[col] * 8.0);
      fdtblUV[k] = 1.0 / (uvTable[ZIGZAG[k]] * aasf[row] * aasf[col] * 8.0);
      k++;
    }
  }
  return { yTable, uvTable, fdtblY, fdtblUV };
}

function fDCTQuant(data, fdtbl) {
  let dataOff = 0;
  for (let i = 0; i < 8; i++) {
    const tmp0 = data[dataOff] + data[dataOff + 7];
    const tmp7 = data[dataOff] - data[dataOff + 7];
    const tmp1 = data[dataOff + 1] + data[dataOff + 6];
    const tmp6 = data[dataOff + 1] - data[dataOff + 6];
    const tmp2 = data[dataOff + 2] + data[dataOff + 5];
    const tmp5 = data[dataOff + 2] - data[dataOff + 5];
    const tmp3 = data[dataOff + 3] + data[dataOff + 4];
    const tmp4 = data[dataOff + 3] - data[dataOff + 4];

    const tmp10 = tmp0 + tmp3;
    const tmp13 = tmp0 - tmp3;
    const tmp11 = tmp1 + tmp2;
    const tmp12 = tmp1 - tmp2;

    data[dataOff] = tmp10 + tmp11;
    data[dataOff + 4] = tmp10 - tmp11;

    const z1 = (tmp12 + tmp13) * 0.707106781;
    data[dataOff + 2] = tmp13 + z1;
    data[dataOff + 6] = tmp13 - z1;

    const tmp10b = tmp4 + tmp5;
    const tmp11b = tmp5 + tmp6;
    const tmp12b = tmp6 + tmp7;
    const z5 = (tmp10b - tmp12b) * 0.382683433;
    const z2 = 0.541196100 * tmp10b + z5;
    const z4 = 1.306562965 * tmp12b + z5;
    const z3 = tmp11b * 0.707106781;

    const z11 = tmp7 + z3;
    const z13 = tmp7 - z3;
    data[dataOff + 5] = z13 + z2;
    data[dataOff + 3] = z13 - z2;
    data[dataOff + 1] = z11 + z4;
    data[dataOff + 7] = z11 - z4;
    dataOff += 8;
  }

  dataOff = 0;
  for (let i = 0; i < 8; i++) {
    const tmp0 = data[dataOff] + data[dataOff + 56];
    const tmp7 = data[dataOff] - data[dataOff + 56];
    const tmp1 = data[dataOff + 8] + data[dataOff + 48];
    const tmp6 = data[dataOff + 8] - data[dataOff + 48];
    const tmp2 = data[dataOff + 16] + data[dataOff + 40];
    const tmp5 = data[dataOff + 16] - data[dataOff + 40];
    const tmp3 = data[dataOff + 24] + data[dataOff + 32];
    const tmp4 = data[dataOff + 24] - data[dataOff + 32];

    const tmp10 = tmp0 + tmp3;
    const tmp13 = tmp0 - tmp3;
    const tmp11 = tmp1 + tmp2;
    const tmp12 = tmp1 - tmp2;

    data[dataOff] = tmp10 + tmp11;
    data[dataOff + 32] = tmp10 - tmp11;

    const z1 = (tmp12 + tmp13) * 0.707106781;
    data[dataOff + 16] = tmp13 + z1;
    data[dataOff + 48] = tmp13 - z1;

    const tmp10b = tmp4 + tmp5;
    const tmp11b = tmp5 + tmp6;
    const tmp12b = tmp6 + tmp7;
    const z5 = (tmp10b - tmp12b) * 0.382683433;
    const z2 = 0.541196100 * tmp10b + z5;
    const z4 = 1.306562965 * tmp12b + z5;
    const z3 = tmp11b * 0.707106781;

    const z11 = tmp7 + z3;
    const z13 = tmp7 - z3;
    data[dataOff + 40] = z13 + z2;
    data[dataOff + 24] = z13 - z2;
    data[dataOff + 8] = z11 + z4;
    data[dataOff + 56] = z11 - z4;
    dataOff++;
  }

  const out = new Int32Array(64);
  for (let i = 0; i < 64; i++) {
    const f = data[i] * fdtbl[i];
    out[ZIGZAG[i]] = f > 0 ? Math.floor(f + 0.5) : Math.ceil(f - 0.5);
  }
  return out;
}

/**
 * Encode an RGB/RGBA Uint8Array into a Baseline JPEG Buffer.
 */
export function encodeRgbToJpeg(pixels, width, height, channels = 3, quality = 85) {
  const out = [];
  let bytenew = 0;
  let bytepos = 7;

  const writeByte = (b) => out.push(b & 0xff);
  const writeWord = (w) => {
    out.push((w >> 8) & 0xff, w & 0xff);
  };
  const writeBits = (bs) => {
    const value = bs[0];
    let posval = bs[1] - 1;
    while (posval >= 0) {
      if (value & (1 << posval)) bytenew |= 1 << bytepos;
      posval--;
      bytepos--;
      if (bytepos < 0) {
        if (bytenew === 0xff) {
          writeByte(0xff);
          writeByte(0x00);
        } else {
          writeByte(bytenew);
        }
        bytepos = 7;
        bytenew = 0;
      }
    }
  };

  const { yTable, uvTable, fdtblY, fdtblUV } = initQuantTables(Math.max(1, Math.min(100, quality)));

  // SOI + APP0 (JFIF)
  writeWord(0xffd8);
  writeWord(0xffe0);
  writeWord(16);
  writeByte(0x4a); writeByte(0x46); writeByte(0x49); writeByte(0x46); writeByte(0x00);
  writeByte(1); writeByte(1); writeByte(0); writeWord(1); writeWord(1); writeByte(0); writeByte(0);

  // DQT
  writeWord(0xffdb);
  writeWord(132);
  writeByte(0);
  for (let i = 0; i < 64; i++) writeByte(yTable[i]);
  writeByte(1);
  for (let i = 0; i < 64; i++) writeByte(uvTable[i]);

  // SOF0
  writeWord(0xffc0);
  writeWord(17);
  writeByte(8);
  writeWord(height);
  writeWord(width);
  writeByte(3);
  writeByte(1); writeByte(0x11); writeByte(0);
  writeByte(2); writeByte(0x11); writeByte(1);
  writeByte(3); writeByte(0x11); writeByte(1);

  // DHT
  const writeHT = (classAndId, nrCodes, values) => {
    writeByte(classAndId);
    for (let i = 1; i <= 16; i++) writeByte(nrCodes[i]);
    for (let i = 0; i < values.length; i++) writeByte(values[i]);
  };
  writeWord(0xffc4);
  const dhtLen = 2 + (1 + 16 + STD_DC_LUMA_VAL.length) +
    (1 + 16 + STD_AC_LUMA_VAL.length) +
    (1 + 16 + STD_DC_CHROMA_VAL.length) +
    (1 + 16 + STD_AC_CHROMA_VAL.length);
  writeWord(dhtLen);
  writeHT(0x00, STD_DC_LUMA_NR, STD_DC_LUMA_VAL);
  writeHT(0x10, STD_AC_LUMA_NR, STD_AC_LUMA_VAL);
  writeHT(0x01, STD_DC_CHROMA_NR, STD_DC_CHROMA_VAL);
  writeHT(0x11, STD_AC_CHROMA_NR, STD_AC_CHROMA_VAL);

  // SOS
  writeWord(0xffda);
  writeWord(12);
  writeByte(3);
  writeByte(1); writeByte(0x00);
  writeByte(2); writeByte(0x11);
  writeByte(3); writeByte(0x11);
  writeByte(0); writeByte(63); writeByte(0);

  const processDU = (CDU, fdtbl, DC, HTDC, HTAC) => {
    const DU = fDCTQuant(CDU, fdtbl);
    const diff = DU[0] - DC;
    if (diff === 0) {
      writeBits(HTDC[0]);
    } else {
      const pos = 32767 + diff;
      writeBits(HTDC[CATEGORY[pos]]);
      writeBits(BITCODE[pos]);
    }
    let end0pos = 63;
    while (end0pos > 0 && DU[end0pos] === 0) end0pos--;
    if (end0pos === 0) {
      writeBits(HTAC[0x00]);
      return DU[0];
    }
    let i = 1;
    while (i <= end0pos) {
      const startpos = i;
      while (DU[i] === 0 && i <= end0pos) i++;
      let nrzeroes = i - startpos;
      if (nrzeroes >= 16) {
        const lng = nrzeroes >> 4;
        for (let n = 1; n <= lng; n++) writeBits(HTAC[0xf0]);
        nrzeroes &= 0x0f;
      }
      const pos = 32767 + DU[i];
      writeBits(HTAC[(nrzeroes << 4) + CATEGORY[pos]]);
      writeBits(BITCODE[pos]);
      i++;
    }
    if (end0pos !== 63) writeBits(HTAC[0x00]);
    return DU[0];
  };

  let DCY = 0;
  let DCU = 0;
  let DCV = 0;
  const YDU = new Float64Array(64);
  const UDU = new Float64Array(64);
  const VDU = new Float64Array(64);

  for (let ypos = 0; ypos < height; ypos += 8) {
    for (let xpos = 0; xpos < width; xpos += 8) {
      let idx = 0;
      for (let y = 0; y < 8; y++) {
        const sy = Math.min(height - 1, ypos + y);
        for (let x = 0; x < 8; x++) {
          const sx = Math.min(width - 1, xpos + x);
          const p = (sy * width + sx) * channels;
          const r = pixels[p];
          const g = pixels[p + 1];
          const b = pixels[p + 2];
          YDU[idx] = (0.299 * r + 0.587 * g + 0.114 * b) - 128;
          UDU[idx] = -0.16874 * r - 0.33126 * g + 0.5 * b;
          VDU[idx] = 0.5 * r - 0.41869 * g - 0.08131 * b;
          idx++;
        }
      }
      DCY = processDU(YDU, fdtblY, DCY, YDC_HT, YAC_HT);
      DCU = processDU(UDU, fdtblUV, DCU, UVDC_HT, UVAC_HT);
      DCV = processDU(VDU, fdtblUV, DCV, UVDC_HT, UVAC_HT);
    }
  }

  if (bytepos >= 0) {
    writeBits([(1 << (bytepos + 1)) - 1, bytepos + 1]);
  }
  writeWord(0xffd9);
  return Buffer.from(out);
}

function clampByte(n) {
  return n < 0 ? 0 : n > 255 ? 255 : n | 0;
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

/**
 * Render rich procedural demo photos (valid baseline JPEGs) for Vita3K offline/demo testing.
 */
export function renderDemoPhotoJpeg(sceneKey, width = 480, height = 360) {
  const buf = new Uint8Array(width * height * 3);
  const palettes = {
    sunset: [[255, 94, 98], [255, 195, 113], [45, 25, 85]],
    ocean: [[18, 194, 233], [196, 113, 237], [246, 79, 89]],
    tokyo: [[15, 32, 39], [32, 58, 67], [240, 80, 140]],
    forest: [[19, 78, 94], [113, 178, 128], [235, 210, 130]],
    vita: [[0, 114, 206], [42, 180, 255], [16, 28, 64]],
    coffee: [[109, 76, 65], [215, 204, 200], [255, 179, 71]],
    mountain: [[44, 62, 80], [253, 116, 108], [255, 144, 104]],
    retro: [[131, 58, 180], [253, 29, 29], [252, 176, 69]],
  };
  const [c1, c2, c3] = palettes[sceneKey] || palettes.vita;

  for (let y = 0; y < height; y++) {
    const ny = y / height;
    for (let x = 0; x < width; x++) {
      const nx = x / width;
      let r = lerp(c1[0], c2[0], ny * 0.8 + nx * 0.2);
      let g = lerp(c1[1], c2[1], ny * 0.8 + nx * 0.2);
      let b = lerp(c1[2], c2[2], ny * 0.8 + nx * 0.2);

      // Sun/moon disc
      const dx = nx - 0.52;
      const dy = (ny - 0.36) * (height / width);
      const dist = Math.hypot(dx, dy);
      if (dist < 0.16) {
        const edge = Math.min(1, (0.16 - dist) * 40);
        r = lerp(r, 255, edge * 0.9);
        g = lerp(g, 238, edge * 0.85);
        b = lerp(b, 175, edge * 0.7);
      }

      // Layered mountain/horizon silhouette
      const ridge1 = 0.62 + 0.08 * Math.sin(nx * 9.0 + 0.5) + 0.04 * Math.cos(nx * 19.0);
      const ridge2 = 0.74 + 0.06 * Math.cos(nx * 7.0 + 1.2) + 0.03 * Math.sin(nx * 15.0);
      if (ny > ridge2) {
        r = c3[0] * 0.65;
        g = c3[1] * 0.65;
        b = c3[2] * 0.65;
      } else if (ny > ridge1) {
        r = c3[0];
        g = c3[1];
        b = c3[2];
      }

      // Subtle scan/grid aesthetic
      if (y % 48 === 0 || x % 48 === 0) {
        r = Math.min(255, r + 8);
        g = Math.min(255, g + 8);
        b = Math.min(255, b + 8);
      }

      const idx = (y * width + x) * 3;
      buf[idx] = clampByte(r);
      buf[idx + 1] = clampByte(g);
      buf[idx + 2] = clampByte(b);
    }
  }

  return encodeRgbToJpeg(buf, width, height, 3, 84);
}

/**
 * Render a circular-styled gradient avatar JPEG.
 */
export function renderAvatarJpeg(seed = 0, size = 96) {
  const buf = new Uint8Array(size * size * 3);
  const hues = [
    [[225, 48, 108], [252, 175, 69]],
    [[64, 93, 230], [131, 58, 180]],
    [[0, 168, 255], [76, 209, 55]],
    [[255, 71, 87], [255, 165, 2]],
    [[55, 66, 250], [112, 161, 255]],
  ];
  const [a, b] = hues[Math.abs(seed) % hues.length];
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const nx = x / size - 0.5;
      const ny = y / size - 0.5;
      const d = Math.hypot(nx, ny);
      const t = (x + y) / (size * 2);
      let r = lerp(a[0], b[0], t);
      let g = lerp(a[1], b[1], t);
      let bl = lerp(a[2], b[2], t);
      // Inner emblem ring
      if (d > 0.36 && d < 0.43) {
        r = 255; g = 255; bl = 255;
      } else if (d >= 0.48) {
        r = 24; g = 24; bl = 30;
      }
      const idx = (y * size + x) * 3;
      buf[idx] = clampByte(r);
      buf[idx + 1] = clampByte(g);
      buf[idx + 2] = clampByte(bl);
    }
  }
  return encodeRgbToJpeg(buf, size, size, 3, 85);
}

