#include <svanes/geometry.hpp>

#include <cmath>

namespace svanes {

Transform ComposeTransforms(const Transform& parent, const Transform& local)
{
    const float cosine = std::cos(parent.rotation);
    const float sine = std::sin(parent.rotation);
    return {
        parent.x + local.x * cosine - local.y * sine,
        parent.y + local.x * sine + local.y * cosine,
        parent.rotation + local.rotation,
    };
}

}
