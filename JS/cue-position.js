// cue-position.js — where a cue sits is decided when the track is read, not
// written in it. Each cue of a zcue track holds one Wirth syntax notation LITERAL,
// in order; its offset is where that literal is found in the source buffer, from
// where the one before it ended. The offset is a pixel of the 65,536 (256 x 256):
// row = offset >> 8 & 255, column = offset & 255, page = offset >> 16.
//   exact:      line is the row, position the column (snapToLines off)
//   hypotenuse: position is the distance from the origin, hypot(row, column)
// The same rows and columns as Media Spectrum DevTools' grid (row = bin >> 4 there).

const ENTITY = { '&amp;': '&', '&lt;': '<', '&gt;': '>' };

// A cue's text back to its source line: tags off, entities back, WSN quotes undone.
export function literalOf(cueText) {
  const wsn = cueText.replace(/<[^>]*>/g, '').replace(/&(amp|lt|gt);/g, (e) => ENTITY[e]);
  return wsn.slice(1, -1).replace(/""/g, '"');
}

function indexOf(buffer, bytes, from) {
  outer: for (let i = from; i + bytes.length <= buffer.length; i++) {
    for (let j = 0; j < bytes.length; j++) if (buffer[i + j] !== bytes[j]) continue outer;
    return i;
  }
  return -1;
}

// Place each cue at its literal's offset; returns the offsets (-1 where a literal is not found).
export function placeCues(cues, buffer, mode = 'exact') {
  const utf8 = new TextEncoder();
  const max = Math.hypot(255, 255);
  let from = 0;
  return Array.from(cues, (cue) => {
    const bytes = utf8.encode(literalOf(cue.text));
    const offset = indexOf(buffer, bytes, from);
    if (offset < 0) return -1;
    from = offset + bytes.length;
    const row = (offset >> 8) & 255, column = offset & 255;
    if (mode === 'exact') {
      cue.snapToLines = false;
      cue.line = (row / 256) * 100;
      cue.lineAlign = 'start';
      cue.position = (column / 256) * 100;
      cue.positionAlign = 'line-left';
    } else {
      cue.line = 'auto';
      cue.position = (Math.hypot(row, column) / max) * 100;
    }
    return offset;
  });
}
