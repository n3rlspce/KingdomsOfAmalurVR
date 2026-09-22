#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
#include <cstring>
namespace amalur {
struct LongswordDebugStatus {
    bool active{},height{},angle{},steady{},preparation{},ready{};
    float progress{},speed{},heightMetres{},upDot{};
    unsigned serial{};uint64_t tick{};
    const char* gate="WAIT";
    bool gripMode{};
};
// Unproject a small view-facing diagnostic into world space so it uses the
// existing stereoized world shader. No second eye offset or screen-line path.
class DebugProjection {
    double inverse_[4][4]{};bool valid_{};
public:
    explicit DebugProjection(const float* matrix){
        double a[4][8]{};
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){if(!std::isfinite(matrix[r*4+c]))return;a[r][c]=matrix[r*4+c];a[r][c+4]=r==c?1:0;}
        for(unsigned i=0;i<4;++i){unsigned pivot=i;for(unsigned r=i+1;r<4;++r)if(std::abs(a[r][i])>std::abs(a[pivot][i]))pivot=r;
            if(std::abs(a[pivot][i])<1e-12)return;
            for(unsigned c=0;c<8;++c){const auto t=a[i][c];a[i][c]=a[pivot][c];a[pivot][c]=t;}
            const auto divisor=a[i][i];for(auto& value:a[i])value/=divisor;
            for(unsigned r=0;r<4;++r)if(r!=i){const auto factor=a[r][i];for(unsigned c=0;c<8;++c)a[r][c]-=factor*a[i][c];}
        }
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)inverse_[r][c]=a[r][c+4];valid_=true;
    }
    bool point(float x,float y,float z,mgs5vr::Vec3& p)const{
        if(!valid_)return false;double in[]{x,y,z,1},out[4]{};
        for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)out[c]+=in[r]*inverse_[r][c];
        if(!std::isfinite(out[3])||std::abs(out[3])<1e-9)return false;
        p={float(out[0]/out[3]),float(out[1]/out[3]),float(out[2]/out[3])};
        return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
    }
};
inline const char* debugGlyph(char c){
    switch(c){
    case 'A':return "010101111101101";case 'B':return "110101110101110";case 'C':return "011100100100011";
    case 'D':return "110101101101110";case 'E':return "111100110100111";case 'F':return "111100110100100";
    case 'G':return "011100101101011";case 'H':return "101101111101101";case 'I':return "111010010010111";
    case 'J':return "001001001101010";case 'K':return "101101110101101";case 'L':return "100100100100111";
    case 'M':return "101111111101101";case 'N':return "101111111111101";case 'O':return "010101101101010";
    case 'P':return "110101110100100";case 'Q':return "010101101111011";case 'R':return "110101110101101";
    case 'S':return "011100010001110";case 'T':return "111010010010010";case 'U':return "101101101101111";
    case 'V':return "101101101101010";case 'W':return "101101111111101";case 'X':return "101101010101101";
    case 'Y':return "101101010010010";case 'Z':return "111001010100111";
    case '0':return "111101101101111";case '1':return "010110010010111";case '2':return "110001010100111";
    case '3':return "110001010001110";case '4':return "101101111001001";case '5':return "111100110001110";
    case '6':return "011100111101111";case '7':return "111001010010010";case '8':return "111101111101111";
    case '9':return "111101111001110";case '.':return "000000000000010";case '-':return "000000111000000";
    case '%':return "101001010100101";case ':':return "000010000010000";default:return "000000000000000";
    }
}
}
