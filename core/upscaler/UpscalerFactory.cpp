#include "UpscalerFactory.h"

namespace aurora::upscaler {
std::unique_ptr<UpscalerBase> UpscalerFactory::create(UpscaleModel model) {
    switch (model) {
    case UpscaleModel::RealESRGAN: return std::make_unique<RealESRGAN>();
    case UpscaleModel::SPAN:       return std::make_unique<SPAN>();
    case UpscaleModel::Anime4K:    return std::make_unique<Anime4K>();
    case UpscaleModel::FSRCNN:     return std::make_unique<FSRCNN>();
    default:                        return std::make_unique<RealESRGAN>();
    }
}
} // namespace aurora::upscaler
