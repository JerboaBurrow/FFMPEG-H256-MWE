#include <iostream>
#include <random>
#include <math.h>
#include <algorithm>

#include <glm/glm.hpp>

#include <H265.h>
#include <caffeine.h>

std::pair<bool, std::pair<glm::vec3, glm::vec3>> sphereIntersection
(
    glm::vec3 center,
    float radius,
    glm::vec3 rayDirector,
    glm::vec3 cameraPosition
)
{
    float b = glm::dot(cameraPosition, rayDirector)-glm::dot(rayDirector, center);
    float r2 = radius*radius;
    float det = b*b - (glm::dot(center, center)+glm::dot(cameraPosition, cameraPosition)-r2-2.0*glm::dot(center, cameraPosition));
    if (det < 0) { return {false, {{NAN, NAN, NAN}, {NAN, NAN, NAN}}}; }
    det = std::sqrt(det);
    glm::vec3 a = (-b+det)*rayDirector;
    glm::vec3 an = a-center+cameraPosition;
    an /= glm::length(an);
    return {true, {a, an}};
}

std::array<Atom, 24> sorted(glm::vec3 camera)
{
    std::array<std::pair<float, uint8_t>, 24> dists;
    for (uint8_t i = 0; i < 24; i++)
    {
        glm::vec3 r = camera-caffeine[i].position;
        dists[i] = {glm::dot(r,r), i};
    }
    std::sort
    (
        dists.begin(),
        dists.end(),
        []
        (
            const std::pair<float, uint8_t> & a,
            const std::pair<float, uint8_t> & b
        )
        {
            return a.first < b.first;
        }
    );
    std::array<Atom, 24> sortedAtoms;
    uint8_t i = 0;
    for (const auto & di : dists)
    {
        sortedAtoms[i] = caffeine[di.second];
        i++;
    }
    return sortedAtoms;
}

glm::vec3 extent(const std::array<Atom, 24> & atoms)
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(-std::numeric_limits<float>::max());
    for (const auto & atom : atoms)
    {
        for (uint8_t i = 0; i < 3; i++)
        {
            min[i] = std::min(min[i], atom.position[i]);
            max[i] = std::max(max[i], atom.position[i]);
        }
    }
    return max-min;
}

int main(int argc, char* argv[])
{
    uint16_t height = 1080;
    uint16_t width = 1080;

    H265Encoder h265("out.mp4", width, height);

    glm::vec3 com = glm::vec3(0.0f);
    for (const Atom & atom : caffeine)
    {
        com += atom.position;
    }
    com /= caffeine.size();
    for (Atom & atom : caffeine)
    {
        atom.position -= com;
        atom.position += glm::vec3(6.0f,6.0f,0.0f);
    }
    glm::vec3 camera = glm::vec3(0.0,0.0,32.0f);
    glm::vec3 ext = extent(caffeine);
    float di = 2.0*ext.y/height;
    float dj = 2.0*ext.x/width;

    std::vector<uint8_t> frame(width * height * 4, 0);

    h265.open(true);
    for (int f = 0; f < 1*60; f++)
    {
        float pi = 0.0;
        auto atoms = sorted(camera);
        for (uint16_t i = 0; i < height; i++)
        {
            float pj = 0.0;
            for (uint16_t j = 0; j < width; j++)
            {
                glm::vec3 ray = glm::vec3(pj, pi, 0.0f)-camera;
                ray /= glm::length(ray);
                frame[i*width*4+j*4] = 5;
                frame[i*width*4+j*4+1] = 5;
                frame[i*width*4+j*4+2] = 5;
                frame[i*width*4+j*4+3] = 255;
                for (auto & atom : atoms)
                {
                    auto posnorm = sphereIntersection
                    (
                        atom.position,
                        atom.radius,
                        ray,
                        camera
                    );
                    if (posnorm.first)
                    {
                        glm::vec4 colour = CPK_COLOURS.at(atom.element);
                        glm::vec3 l = camera-atom.position;
                        l /= glm::length(l);
                        float lighting = 0.0001+0.75*std::max(glm::dot(-posnorm.second.second,l), 0.0f);
                        frame[i*width*4+j*4] = uint8_t(255*colour.r*lighting);
                        frame[i*width*4+j*4+1] = uint8_t(255*colour.g*lighting);
                        frame[i*width*4+j*4+2] = uint8_t(255*colour.b*lighting);
                        frame[i*width*4+j*4+3] = 255;
                        break;
                    }
                }
                pj += dj;
            }
            pi += di;
        }
        h265.write(frame.data());
    }

    h265.finish();
    return 0;
}