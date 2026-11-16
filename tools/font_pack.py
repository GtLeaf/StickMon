"""Lossless transport for legacy SMFN bitmap and A4 font records."""
import struct
import zlib

HEADER = struct.Struct('<IHHBBBBI')
EXTENSION = struct.Struct('<III')
COMPRESSED = 0x80000000

def unpack_font(data):
    if len(data) < HEADER.size:
        raise ValueError('Truncated font header')
    magic, version, count, width, height, stride, reserved, flags = HEADER.unpack_from(data)
    base_flags = flags & ~COMPRESSED
    valid_shape = ((version, width, height, stride, base_flags) in
                   [(1,16,16,32,0),(1,32,32,128,0),
                    (2,16,16,128,4),(2,20,20,200,4)])
    if magic != 0x4e464d53 or reserved or not 0 < count <= 2048 or not valid_shape:
        raise ValueError('Invalid font header')
    expected = count * (4 + stride)
    if flags & COMPRESSED:
        if len(data) < 28:
            raise ValueError('Truncated compressed font header')
        decoded, encoded, crc = EXTENSION.unpack_from(data,16)
        if decoded != expected or not encoded or len(data) != 28 + encoded:
            raise ValueError('Invalid compressed font length')
        try:
            stream = zlib.decompressobj(-15)
            body = stream.decompress(data[28:], expected + 1)
        except zlib.error as error:
            raise ValueError('Invalid compressed font stream') from error
        if (len(body) != expected or not stream.eof or stream.unused_data or
                stream.unconsumed_tail or zlib.crc32(body) != crc):
            raise ValueError('Invalid compressed font payload')
    else:
        body = data[16:]
        if len(body) != expected:
            raise ValueError('Invalid font record length')
    return HEADER.pack(magic,version,count,width,height,stride,reserved,base_flags) + body

def compress_font(data):
    raw = unpack_font(data)
    h = bytearray(raw[:16]); body = raw[16:]
    struct.pack_into('<I',h,12,struct.unpack_from('<I',h,12)[0] | COMPRESSED)
    compressor = zlib.compressobj(9,wbits=-15)
    encoded = compressor.compress(body) + compressor.flush()
    return bytes(h) + EXTENSION.pack(len(body),len(encoded),zlib.crc32(body)) + encoded
