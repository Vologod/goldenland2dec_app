#pragma once
#include "gl/assets/iad.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/assets/wear_had.hpp"
#include "gl/assets/wear_seq.hpp"
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace gl::game {

struct WearLayerImages {
    const gl::assets::IadBlock *block = nullptr;
    const gl::assets::Image *body = nullptr;
    const gl::assets::Image *shadow = nullptr;
    const gl::assets::WearSequence *seq = nullptr;
};

gl::assets::Image compose_wear_body(const gl::assets::WearHadAnimation &animation,
                                    const gl::assets::Image &base,
                                    std::span<const WearLayerImages> layers);
gl::assets::Image compose_wear_shadow(const gl::assets::WearHadAnimation &animation,
                                      const gl::assets::Image &base,
                                      std::span<const WearLayerImages> layers);

struct HeroOutfit {
    std::string wear_set = "noweapon_thrw";
    std::string weapon;
    std::string shield;
    std::string elbarms = "ponogi_goluboy";
    std::string helmet = "shlem_goluboy";
    std::string armor = "dospeh_almaz";
};

struct HeroAnimation {
    gl::assets::WearHadAnimation header;
    gl::assets::Image body;
    gl::assets::Image shadow;
};

class HeroVisual {
  public:
    const HeroAnimation *find(std::uint32_t type_mask) const {
        auto it = animations_.find(type_mask);
        return it == animations_.end() ? nullptr : &it->second;
    }
    const HeroOutfit &outfit() const { return outfit_; }
    const std::map<std::uint32_t, HeroAnimation> &animations() const { return animations_; }

  private:
    friend Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs &, const HeroOutfit &);
    std::map<std::uint32_t, HeroAnimation> animations_;
    HeroOutfit outfit_;
};

Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs &vfs, const HeroOutfit &outfit);

} // namespace gl::game
