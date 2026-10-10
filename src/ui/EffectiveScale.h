#pragma once
#include <algorithm>
#include <cmath>
namespace harmony::ui {
struct EffectiveScale {
    double host{1.},user{1.};
    EffectiveScale(double content,double zoom):
        host(std::isfinite(content)&&content>=.5&&content<=4?content:1.),
        user(std::isfinite(zoom)?std::clamp(zoom,1.,1.5):1.){}
    // VSTGUI already transforms OS pointer positions by host scale. MainView
    // consumes those view coordinates, then applies user zoom exactly once.
    double physicalEditor(double logicalEditor) const noexcept{return logicalEditor*host;}
    double logicalEditor(double physical) const noexcept{return physical/host;}
    double logicalContent(double viewCoordinate) const noexcept{return viewCoordinate/user;}
    double physicalContent(double logical) const noexcept{return logical*host*user;}
};
}
