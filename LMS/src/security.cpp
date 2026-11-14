// Library Management System - security.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
namespace lms {
    namespace {
        constexpr std::array<uint32_t,64> k = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
    // Rotate a 32-bit word as required by SHA-256.
        uint32_t rotate(uint32_t x,int n) {
            return (x>>n)|(x<<(32-n));
        }
        using Digest=std::array<uint8_t,32>;
    // Compute a SHA-256 digest over the input bytes.
        Digest sha256(const std::vector<uint8_t>& bytes) {
            std::vector<uint8_t> data=bytes;
            const uint64_t bitLength=static_cast<uint64_t>(data.size())*8;
            data.push_back(0x80);
            while(data.size()%64!=56) data.push_back(0);
            for(int i=7;i>=0;--i) data.push_back(static_cast<uint8_t>(bitLength>>(i*8)));
            std::array<uint32_t,8> h= {
                0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
            };
            for(size_t off=0;off<data.size();off+=64) {
                uint32_t w[64] {
                };
                for(int i=0;i<16;++i) {
                    size_t j=off+static_cast<size_t>(i)*4;
                    w[i]=(uint32_t(data[j])<<24)|(uint32_t(data[j+1])<<16)|(uint32_t(data[j+2])<<8)|uint32_t(data[j+3]);
                }
                for(int i=16;i<64;++i) {
                    auto a=w[i-15],b=w[i-2];
                    w[i]=(rotate(b,17)^rotate(b,19)^(b>>10))+w[i-7]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-16];
                }
                auto [a,b,c,d,e,f,g,hh]=h;
                for(int i=0;i<64;++i) {
                    uint32_t s1=rotate(e,6)^rotate(e,11)^rotate(e,25),ch=(e&f)^(~e&g);
                    uint32_t t1=hh+s1+ch+k[i]+w[i];
                    uint32_t s0=rotate(a,2)^rotate(a,13)^rotate(a,22),maj=(a&b)^(a&c)^(b&c);
                    uint32_t t2=s0+maj;
                    hh=g;
                    g=f;
                    f=e;
                    e=d+t1;
                    d=c;
                    c=b;
                    b=a;
                    a=t1+t2;
                }
                h[0]+=a;
                h[1]+=b;
                h[2]+=c;
                h[3]+=d;
                h[4]+=e;
                h[5]+=f;
                h[6]+=g;
                h[7]+=hh;
            }
            Digest out {
            };
            for(int i=0;i<8;++i)for(int j=0;j<4;++j)out[static_cast<size_t>(i)*4+j]=static_cast<uint8_t>(h[i]>>(24-8*j));
            return out;
        }
    // Compute HMAC-SHA256 for password key derivation.
        Digest hmac(const std::vector<uint8_t>& key,const std::vector<uint8_t>& message) {
            std::vector<uint8_t> block(64,0);
            if(key.size()>64) {
                auto digest=sha256(key);
                std::copy(digest.begin(),digest.end(),block.begin());
            }
            else std::copy(key.begin(),key.end(),block.begin());
            std::vector<uint8_t> inner(64),outer(64);
            for(size_t i=0;i<64;++i) {
                inner[i]=block[i]^0x36;
                outer[i]=block[i]^0x5c;
            }
            inner.insert(inner.end(),message.begin(),message.end());
            auto first=sha256(inner);
            outer.insert(outer.end(),first.begin(),first.end());
            return sha256(outer);
        }
    // Convert hexadecimal text into a byte vector.
        std::vector<uint8_t> unhex(const std::string& s) {
            std::vector<uint8_t> out;
            for(size_t i=0;i+1<s.size();i+=2)out.push_back(static_cast<uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
            return out;
        }
    // Encode a byte sequence as lowercase hexadecimal text.
        std::string hex(const uint8_t* data,size_t n) {
            std::ostringstream os;
            for(size_t i=0;i<n;++i)os<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(data[i]);
            return os.str();
        }
    }
    // Generate a random salt for a new user account.
    std::string newSalt() {
        std::random_device rd;
        std::array<uint8_t,16> b {
        };
        for(auto& v:b)v=static_cast<uint8_t>(rd());
        return hex(b.data(),b.size());
    }
    // Derive a password hash using PBKDF2-HMAC-SHA256.
    std::string hashPassword(const std::string& password,const std::string& salt) {
        constexpr int iterations=120000;
        std::vector<uint8_t> key(password.begin(),password.end()),msg=unhex(salt);
        msg.insert(msg.end(), {
            0,0,0,1
        });
        auto u=hmac(key,msg),result=u;
        for(int i=1;i<iterations;++i) {
            u=hmac(key,std::vector<uint8_t>(u.begin(),u.end()));
            for(size_t j=0;j<result.size();++j)result[j]^=u[j];
        }
        return hex(result.data(),result.size());
    }
}
