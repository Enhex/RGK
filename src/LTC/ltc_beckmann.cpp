#include "ltc_beckmann.hpp"
#include "../utils.hpp"
#include <algorithm>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

namespace LTC_BECKMANN{
	extern const int size;
	extern const mat33 tabM[];
	extern const float tabAmplitude[];

glm::mat3 get_n(int theta, int alpha){
    return tabM[alpha + theta*size];
}
float getAMP_n(int theta, int alpha){
    return tabAmplitude[alpha + theta*size];
}

std::pair<glm::mat3, float> get_bilinear(const float theta, const float alpha)
{
    float t = glm::max(0.0f, glm::min(1.0f, theta / (0.5f*3.14159f)));
    float a = glm::max(0.0f, glm::min(1.0f, sqrtf(alpha)));
    if(t >= 1.0f) t = 0.999f;
    if(a >= 1.0f) a = 0.999f;
    int t1 = floorf(t * size);
    int t2 = t1 + 1;
    int a1 = floorf(a * size);
    int a2 = a1 + 1;
    glm::mat3 Mt1a1 = get_n(t1,a1);
    glm::mat3 Mt1a2 = get_n(t1,a2);
    glm::mat3 Mt2a1 = get_n(t2,a1);
    glm::mat3 Mt2a2 = get_n(t2,a2);
    float At1a1 = getAMP_n(t1,a1);
    float At1a2 = getAMP_n(t1,a2);
    float At2a1 = getAMP_n(t2,a1);
    float At2a2 = getAMP_n(t2,a2);

    float dt1 = t*size - t1;
    float dt2 = t2 - t*size;
    float da1 = a*size - a1;
    float da2 = a2 - a*size;

    glm::mat3 resM =
        Mt1a1 * dt2 * da2 +
        Mt1a2 * dt2 * da1 +
        Mt2a1 * dt1 * da2 +
        Mt2a2 * dt1 * da1;
    float resAMP =
        At1a1 * dt2 * da2 +
        At1a2 * dt2 * da1 +
        At2a1 * dt1 * da2 +
        At2a2 * dt1 * da1;

	return {resM, resAMP};
}

float getAMP(const float theta, const float alpha)
{
	int t = std::max<int>(0, std::min<int>(size-1, (int)floorf(theta / (0.5f*3.14159f) * size)));
	int a = std::max<int>(0, std::min<int>(size-1, (int)floorf(sqrtf(alpha) * size)));

	return tabAmplitude[a + t*size];
}

static glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest){
	start = normalize(start);
	dest = normalize(dest);

	float cosTheta = dot(start, dest);
    glm::vec3 rotationAxis;

	if (cosTheta < -1 + 0.001f){
		// special case when vectors in opposite directions:
		// there is no "ideal" rotation axis
		// So guess one; any will do as long as it's perpendicular to start
		rotationAxis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), start);
		if (glm::length(rotationAxis) < 0.01 ) // bad luck, they were parallel, try again!
			rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);

		rotationAxis = normalize(rotationAxis);
        //std::cout << "Rotation axis = " << rotationAxis << std::endl;
		return glm::angleAxis(glm::pi<float>(), rotationAxis);
	}

	rotationAxis = cross(start, dest);

	float s = sqrt( (1+cosTheta)*2 );
	float invs = 1 / s;

	return glm::quat(
		s * 0.5f,
		rotationAxis.x * invs,
		rotationAxis.y * invs,
		rotationAxis.z * invs
	);

}

float get_pdf(glm::vec3 N, glm::vec3 Vr, glm::vec3 Vi, float alpha, bool debug){
    assert(alpha >= 0.0f && alpha <= 1.0f);
    (void)debug;
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::quat N_to_up = RotationBetweenVectors(N, up);
    //glm::vec3 N2 = N_to_up * N;
    glm::vec3 Vi2 = N_to_up * Vi;
    glm::vec3 Vr2 = N_to_up * Vr;
    // Rotation 2
    glm::vec3 front(1.0f, 0.0f, 0.0f);
    glm::vec3 Vi_cast(Vi2.x, Vi2.y, 0.0f);
    glm::quat Vi_to_front = RotationBetweenVectors(Vi_cast, front);
    // glm::vec3 N3 = Vi_to_front * N2;
    glm::vec3 Vi3 = Vi_to_front * Vi2;
    glm::vec3 Vr3 = Vi_to_front * Vr2;
    float theta = glm::angle(Vi3, up);
    auto q = get_bilinear(theta, alpha);
    glm::mat3 M = q.first;
    float amplitude = q.second;
    glm::mat3 invM = glm::inverse(M);
    glm::vec3 p = glm::normalize( invM * Vr3 );
    glm::vec3 Loriginal = p;
    glm::vec3 L_ = M * Loriginal;
    float l = glm::length(L_);
    float detM = glm::determinant(M);
	float Jacobian = detM / (l*l*l);
    float D = 1.0f / 3.14159f * glm::max<float>(0.0f, Loriginal.z);
    float res = amplitude * D / Jacobian;
    return res;
}

    glm::vec3 get_random(glm::vec3 N, glm::vec3 Vi, float roughness, glm::vec3 rand_hscos, bool debug){
        glm::vec3 up(0.0f, 0.0f, 1.0f);
        glm::quat N_to_up = RotationBetweenVectors(N, up);
        glm::vec3 Vi2 = N_to_up * Vi;
        glm::vec3 front(1.0f, 0.0f, 0.0f);
        glm::vec3 Vi_cast(Vi2.x, Vi2.y, 0.0f);
        glm::quat Vi_to_front = RotationBetweenVectors(Vi_cast, front);
        glm::quat up_to_N = glm::inverse(N_to_up);
        glm::quat front_to_Vi = glm::inverse(Vi_to_front);

        float theta = glm::angle(Vi, N);
        auto q = get_bilinear(glm::max(theta, glm::pi<float>()/4.0f), roughness);
        glm::mat3 M = q.first;

        IFDEBUG std::cout << "N = " << N << ", Vi = " << Vi << std::endl;
        IFDEBUG std::cout << "theta = " << theta << ", alpha = " << roughness << std::endl;
        IFDEBUG std::cout << "M = " << glm::to_string(M) << std::endl;

        IFDEBUG std::cout << "rand_hscos = " << rand_hscos << std::endl;
        glm::vec3 s = M*rand_hscos;
        IFDEBUG std::cout << "s1 = " << glm::normalize(s) << std::endl;
        s = up_to_N * (front_to_Vi * (s));
        return glm::normalize(s);
    }

} // namespace LTC_BECKMANN
