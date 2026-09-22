#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace amalur {
// Row-vector native matrices: observed WVP must equal world * candidate VP.
// This checks evidence without inverting a large translated world matrix.
inline double cameraMatrixResidual(const float* world16,const float* wvp16,const float* candidateVP16){
    const double rejected=std::numeric_limits<double>::infinity();
    if(!world16||!wvp16||!candidateVP16)return rejected;
    for(unsigned i=0;i<16;++i)
        if(!std::isfinite(world16[i])||!std::isfinite(wvp16[i])||!std::isfinite(candidateVP16[i]))return rejected;
    if(std::abs(world16[3])>1e-4||std::abs(world16[7])>1e-4||std::abs(world16[11])>1e-4||std::abs(double(world16[15])-1)>1e-4)return rejected;
    // Normalize before determinant evaluation so tiny/large uniform scales
    // do not change whether the linear transform is invertible.
    double scale=0;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)scale=std::max(scale,std::abs(double(world16[r*4+c])));
    if(scale==0)return rejected;
    double a[9];for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)a[r*3+c]=world16[r*4+c]/scale;
    const double determinant=a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+a[2]*(a[3]*a[7]-a[4]*a[6]);
    if(std::abs(determinant)<=1e-12)return rejected;
    const double perspective=std::hypot(double(wvp16[3]),double(wvp16[7]),double(wvp16[11]));
    if(perspective<=1e-4)return rejected;
    double residual=0;
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        double predicted=0;
        for(unsigned k=0;k<4;++k)predicted+=double(world16[r*4+k])*candidateVP16[k*4+c];
        const double observed=wvp16[r*4+c];
        residual=std::max(residual,std::abs(predicted-observed)/std::max(1.0,std::abs(observed)));
    }
    return residual;
}
}
