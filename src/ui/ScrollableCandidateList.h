#pragma once
#include "UILayout.h"
#include <optional>

namespace harmony::ui {
// Shared viewport, row placement, hit testing and scrollbar for all seven groups.
// Rows keep their original indices; no recycled candidate views or copied identities.
struct ScrollableCandidateList {
    UiRect viewport;
    double rowHeight{}, offset{}, maximum{};
    std::size_t count{};
    ScrollableCandidateList(UiRect lane,std::size_t size,double scroll,double header=27)
        :viewport{lane.left,lane.top+header,lane.right,lane.bottom-3},count(size) {
        rowHeight=std::max(1.,viewport.height()/2.);
        maximum=std::max(0.,static_cast<double>(count)*rowHeight-viewport.height());
        offset=std::clamp(std::isfinite(scroll)?scroll:0.,0.,maximum);
    }
    double top(std::size_t index) const {return viewport.top+index*rowHeight-offset;}
    bool visible(std::size_t index) const {
        return index<count&&top(index)<viewport.bottom&&top(index)+rowHeight>viewport.top;
    }
    std::optional<std::size_t> hit(double x,double y) const {
        if(!viewport.contains(x,y)||x>=viewport.right-6)return {};
        const auto index=static_cast<std::size_t>((y-viewport.top+offset)/rowHeight);
        if(index>=count)return {};
        return index;
    }
    double scroll(double delta) const {return std::clamp(offset-delta,0.,maximum);}
    UiRect thumb() const {
        if(maximum<=0)return {};
        const auto height=std::min(viewport.height(),std::max(16.,viewport.height()*2./count));
        const auto y=viewport.top+(viewport.height()-height)*offset/maximum;
        return {viewport.right-4,y,viewport.right-1,y+height};
    }
    CandidateRowGeometry continuationRow(UiRect lane,std::size_t index,bool exports) const {
        const double shift=static_cast<double>(index)*rowHeight-offset;
        lane.top+=shift;lane.bottom+=shift;
        return candidateRowGeometry(lane,0,exports);
    }
};
struct EnrichmentRowGeometry {
    UiRect row,summary,score,badges,pin,why,audition,midi,snapshot;
};
inline EnrichmentRowGeometry enrichmentRowGeometry(UiRect lane,const ScrollableCandidateList& list,std::size_t index) {
    const double y=list.top(index),actionY=y+list.rowHeight-30;
    return {{lane.left+6,y,lane.right-6,y+list.rowHeight-2},
        {lane.left+12,y+3,lane.right-48,y+25},{lane.right-46,y+3,lane.right-10,y+25},
        {lane.left+12,y+26,lane.right-12,y+48},
        {lane.right-213,actionY,lane.right-170,actionY+24},
        {lane.right-169,actionY,lane.right-126,actionY+24},
        {lane.right-122,actionY,lane.right-98,actionY+24},
        {lane.right-94,actionY,lane.right-55,actionY+24},
        {lane.right-51,actionY,lane.right-8,actionY+24}};
}
}
