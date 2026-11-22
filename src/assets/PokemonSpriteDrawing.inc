// Shared, non-generated implementation. Included inside PokemonSprites.
namespace {
bool drawTransformedFrame(const SpriteFrame* frame, int x, int y, float scale,
                          bool flipX, bool nearest, Ui::TransformedSpriteRle::Mode mode,
                          uint16_t color = 0, uint8_t amount = 0) {
    const uint8_t width = FlashStorage::readByte(&frame->width);
    const uint8_t height = FlashStorage::readByte(&frame->height);
    const uint8_t imageWidth = FlashStorage::readByte(&frame->imageWidth);
    const uint8_t imageHeight = FlashStorage::readByte(&frame->imageHeight);
    const uint8_t format = FlashStorage::readByte(&frame->format);
    if (FlashStorage::readByte(&frame->source) != SPRITE_SOURCE_FILE_BLOCK ||
        format > static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) return false;
    CachedSpecies* cached = cachedSpeciesFor(FlashStorage::readWord(&frame->speciesId));
    const uint32_t offset = FlashStorage::readDword(&frame->offset);
    const uint32_t length = FlashStorage::readDword(&frame->length);
    if (!cached || offset > cached->rleWords || length > cached->rleWords - offset) return false;
    const bool indexed = format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE);
    const uint32_t paletteOffset = FlashStorage::readDword(&frame->paletteOffset);
    const uint8_t paletteSize = FlashStorage::readByte(&frame->paletteSize);
    if (mode != Ui::TransformedSpriteRle::Mode::Solid && indexed &&
        (paletteOffset > cached->paletteWords || paletteSize > cached->paletteWords - paletteOffset)) return false;
    const Ui::TintedSpriteRle::Source source{cached->data, offset, length,
        cached->palettes, paletteOffset, paletteSize, imageWidth, imageHeight, indexed};
    const Ui::TransformedSpriteRle::Geometry geometry{width, height,
        static_cast<int16_t>(FlashStorage::readWord(&frame->imageOffsetX)),
        static_cast<int16_t>(FlashStorage::readWord(&frame->imageOffsetY)),
        FlashStorage::readByte(&frame->imageFlipX) != 0};
    return Ui::TransformedSpriteRle::draw(PixelRenderer::canvas(), source, geometry,
        x, y, scale, flipX, nearest, mode, color, amount);
}
} // namespace

bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX) {
    if (!frame) return false;
    if (FlashStorage::readByte(&frame->imageWidth))
        return drawTransformedFrame(frame, x, y, 1.0f, flipX, false, Ui::TransformedSpriteRle::Mode::Normal);

    uint8_t width = FlashStorage::readByte(&frame->width);
    uint8_t height = FlashStorage::readByte(&frame->height);
    uint32_t offset = FlashStorage::readDword(&frame->offset);
    uint32_t length = FlashStorage::readDword(&frame->length);
    if (width == 0 || height == 0 || length == 0) return false;

    uint8_t format = FlashStorage::readByte(&frame->format);
    uint8_t source = FlashStorage::readByte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = FlashStorage::readWord(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = FlashStorage::readDword(&frame->paletteOffset);
        uint8_t paletteSize = FlashStorage::readByte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4Rle(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565Rle(x, y, width, height, rle, offset, length, flipX);
    return true;
}

bool drawFrameSilhouette(const SpriteFrame* frame, int x, int y,
                         uint16_t color, bool flipX) {
    if (!frame) return false;
    if (FlashStorage::readByte(&frame->imageWidth))
        return drawTransformedFrame(frame, x, y, 1.0f, flipX, false, Ui::TransformedSpriteRle::Mode::Solid, color);

    uint8_t width = FlashStorage::readByte(&frame->width);
    uint8_t height = FlashStorage::readByte(&frame->height);
    uint32_t offset = FlashStorage::readDword(&frame->offset);
    uint32_t length = FlashStorage::readDword(&frame->length);
    if (width == 0 || height == 0 || length == 0) return false;

    uint8_t format = FlashStorage::readByte(&frame->format);
    uint8_t source = FlashStorage::readByte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = FlashStorage::readWord(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        PixelRenderer::drawIndexed4RleSolid(
            x, y, width, height, cached->data, offset, length, color, flipX);
        return true;
    }
    PixelRenderer::drawRgb565RleSolid(
        x, y, width, height, cached->data, offset, length, color, flipX);
    return true;
}

bool drawFrameTintedScaled(const SpriteFrame* frame, int x, int y, float scale,
                           uint16_t tint, uint8_t amount, bool flipX) {
    if (!frame) return false;
    if (FlashStorage::readByte(&frame->imageWidth))
        return drawTransformedFrame(frame, x, y, scale, flipX, true, Ui::TransformedSpriteRle::Mode::Tinted, tint, amount);
    const uint8_t format = FlashStorage::readByte(&frame->format);
    if (FlashStorage::readByte(&frame->source) != SPRITE_SOURCE_FILE_BLOCK ||
        format > static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) return false;
    CachedSpecies* cached = cachedSpeciesFor(FlashStorage::readWord(&frame->speciesId));
    const uint32_t offset = FlashStorage::readDword(&frame->offset);
    const uint32_t length = FlashStorage::readDword(&frame->length);
    if (!cached || offset > cached->rleWords || length > cached->rleWords - offset) return false;
    const bool indexed = format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE);
    const uint32_t paletteOffset = FlashStorage::readDword(&frame->paletteOffset);
    const uint8_t paletteSize = FlashStorage::readByte(&frame->paletteSize);
    if (indexed && (paletteOffset > cached->paletteWords ||
        paletteSize > cached->paletteWords - paletteOffset)) return false;
    const Ui::TintedSpriteRle::Source source{cached->data, offset, length,
        cached->palettes, paletteOffset, paletteSize,
        FlashStorage::readByte(&frame->width), FlashStorage::readByte(&frame->height), indexed};
    return Ui::TintedSpriteRle::draw(PixelRenderer::canvas(), source, x, y, scale, tint, amount, flipX);
}

bool drawFrameScaled(const SpriteFrame* frame, int x, int y, float scale, bool flipX, bool nearest) {
    if (!frame) return false;
    if (FlashStorage::readByte(&frame->imageWidth))
        return drawTransformedFrame(frame, x, y, scale, flipX, nearest, Ui::TransformedSpriteRle::Mode::Normal);
    if (scale == 1.0f) return drawFrame(frame, x, y, flipX);

    uint8_t width = FlashStorage::readByte(&frame->width);
    uint8_t height = FlashStorage::readByte(&frame->height);
    uint32_t offset = FlashStorage::readDword(&frame->offset);
    uint32_t length = FlashStorage::readDword(&frame->length);
    if (width == 0 || height == 0 || length == 0 || scale <= 0.0f) return false;

    uint8_t format = FlashStorage::readByte(&frame->format);
    uint8_t source = FlashStorage::readByte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = FlashStorage::readWord(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = FlashStorage::readDword(&frame->paletteOffset);
        uint8_t paletteSize = FlashStorage::readByte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4RleScaled(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            scale,
            flipX, 255, nearest);
        return true;
    }

    PixelRenderer::drawRgb565RleScaled(x, y, width, height, rle, offset, length, scale, flipX, nearest);
    return true;
}
