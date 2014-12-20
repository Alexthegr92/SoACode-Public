#include "stdafx.h"
#include "PlanetLoader.h"

#include "NoiseShaderCode.hpp"
#include "Errors.h"

#include <IOManager.h>

enum class TerrainFunction {
    RIDGED_NOISE
};
KEG_ENUM_INIT_BEGIN(TerrainFunction, TerrainFunction, e);
e->addValue("ridgedNoise", TerrainFunction::RIDGED_NOISE);
KEG_ENUM_INIT_END

class TerrainFuncKegProperties {
public:
    TerrainFunction func;
    int octaves = 1;
    float persistence = 1.0f;
    float frequency = 1.0f;
    float low = -1.0f;
    float high = 1.0f;
};
KEG_TYPE_INIT_BEGIN_DEF_VAR(TerrainFuncKegProperties)
KEG_TYPE_INIT_ADD_MEMBER(TerrainFuncKegProperties, I32, octaves);
KEG_TYPE_INIT_ADD_MEMBER(TerrainFuncKegProperties, F32, persistence);
KEG_TYPE_INIT_ADD_MEMBER(TerrainFuncKegProperties, F32, frequency);
KEG_TYPE_INIT_ADD_MEMBER(TerrainFuncKegProperties, F32, low);
KEG_TYPE_INIT_ADD_MEMBER(TerrainFuncKegProperties, F32, high);
KEG_TYPE_INIT_END

class TerrainFuncs {
public:
    std::vector<TerrainFuncKegProperties> funcs;
    float baseHeight = 0.0f;
};

PlanetLoader::PlanetLoader(IOManager* ioManager) :
    m_iom(ioManager) {
    // Empty
}


PlanetLoader::~PlanetLoader() {
}

PlanetGenData* PlanetLoader::loadPlanet(const nString& filePath) {
    nString data;
    m_iom->readFileToString(filePath.c_str(), data);

    YAML::Node node = YAML::Load(data.c_str());
    if (node.IsNull() || !node.IsMap()) {
        std::cout << "Failed to load " + filePath;
        return false;
    }

    TerrainFuncs baseTerrainFuncs;
    TerrainFuncs tempTerrainFuncs;
    TerrainFuncs humTerrainFuncs;

    for (auto& kvp : node) {
        nString type = kvp.first.as<nString>();
        // Parse based on type
        if (type == "baseHeight") {
            parseTerrainFuncs(&baseTerrainFuncs, kvp.second);
        } else if (type == "temperature") {
            parseTerrainFuncs(&tempTerrainFuncs, kvp.second);
        } else if (type == "humidity") {
            parseTerrainFuncs(&humTerrainFuncs, kvp.second);
        }
    }

    // Generate the program
    vg::GLProgram* program = generateProgram(baseTerrainFuncs,
                                            tempTerrainFuncs,
                                            humTerrainFuncs);

    if (program != nullptr) {
        PlanetGenData* genData = new PlanetGenData;
        genData->program = program;
        return genData;
    }
    return nullptr;
}

PlanetGenData* PlanetLoader::getDefaultGenData() {
    // Lazily construct default data
    if (!m_defaultGenData) {
        // Allocate data
        m_defaultGenData = new PlanetGenData;

        // Build string
        nString fSource = NOISE_SRC_FRAG;
        fSource.reserve(fSource.size() + 128);
        fSource += N_HEIGHT + "= 0;";
        fSource += N_TEMP + "= 0;";
        fSource += N_HUM + "= 0; }";

        // Create the shader
        vg::GLProgram* program = new vg::GLProgram;
        program->init();
        program->addShader(vg::ShaderType::VERTEX_SHADER, NOISE_SRC_VERT.c_str());
        program->addShader(vg::ShaderType::FRAGMENT_SHADER, fSource.c_str());
        program->bindFragDataLocation(0, N_HEIGHT.c_str());
        program->bindFragDataLocation(1, N_TEMP.c_str());
        program->bindFragDataLocation(2, N_HUM.c_str());
        program->link();
        
        if (!program->getIsLinked()) {
            std::cout << fSource << std::endl;
            showMessage("Failed to generate default program");
            return nullptr;
        }

        program->initAttributes();
        program->initUniforms();

        m_defaultGenData->program = program;

    }
    return m_defaultGenData;
}

void PlanetLoader::parseTerrainFuncs(TerrainFuncs* terrainFuncs, YAML::Node& node) {
    if (node.IsNull() || !node.IsMap()) {
        std::cout << "Failed to parse node";
        return;
    }

    Keg::Error error;

    for (auto& kvp : node) {
        nString type = kvp.first.as<nString>();
        if (type == "base") {
            terrainFuncs->baseHeight += kvp.second.as<float>();
            continue;
        }

        terrainFuncs->funcs.push_back(TerrainFuncKegProperties());
        if (type == "ridgedNoise") {
            terrainFuncs->funcs.back().func = TerrainFunction::RIDGED_NOISE;
        }

        error = Keg::parse((ui8*)&terrainFuncs->funcs.back(), kvp.second, Keg::getGlobalEnvironment(), &KEG_GLOBAL_TYPE(TerrainFuncKegProperties));
        if (error != Keg::Error::NONE) {
            fprintf(stderr, "Keg error %d\n", (int)error); 
            return;
        }
    }
}

vg::GLProgram* PlanetLoader::generateProgram(TerrainFuncs& baseTerrainFuncs,
                               TerrainFuncs& tempTerrainFuncs,
                               TerrainFuncs& humTerrainFuncs) {
    // Build initial string
    nString fSource = NOISE_SRC_FRAG;
    fSource.reserve(fSource.size() + 8192);

    // Set initial values
    fSource += N_HEIGHT + "=" + std::to_string(baseTerrainFuncs.baseHeight) + ";";
    fSource += N_TEMP + "=" + std::to_string(tempTerrainFuncs.baseHeight) + ";";
    fSource += N_HUM + "=" + std::to_string(humTerrainFuncs.baseHeight) + ";";

    // Add all the noise functions
    addNoiseFunctions(fSource, N_HEIGHT, baseTerrainFuncs);
    addNoiseFunctions(fSource, N_TEMP, tempTerrainFuncs);
    addNoiseFunctions(fSource, N_HUM, humTerrainFuncs);

    // Add final brace
    fSource += "}";

    // Create the shader
    vg::GLProgram* program = new vg::GLProgram;
    program->init();
    program->addShader(vg::ShaderType::VERTEX_SHADER, NOISE_SRC_VERT.c_str());
    program->addShader(vg::ShaderType::FRAGMENT_SHADER, fSource.c_str());
    program->bindFragDataLocation(0, N_HEIGHT.c_str());
    program->bindFragDataLocation(1, N_TEMP.c_str());
    program->bindFragDataLocation(2, N_HUM.c_str());
    program->link();

    if (!program->getIsLinked()) {
        std::cout << fSource << std::endl;
        showMessage("Failed to generate shader program");
        return nullptr;
    }

    program->initAttributes();
    program->initUniforms();

    return program;
}

void PlanetLoader::addNoiseFunctions(nString& fSource, const nString& variable, const TerrainFuncs& funcs) {
#define TS(x) (std::to_string(x))
    // Conditional scaling code. Generates (total / maxAmplitude) * (high - low) * 0.5 + (high + low) * 0.5;
#define SCALE_CODE ((fn.low != -1.0f || fn.high != 1.0f) ? \
    fSource += variable + "+= (total / maxAmplitude) * (" + \
        TS(fn.high) + " - " + TS(fn.low) + ") * 0.5 + (" + TS(fn.high) + " + " + TS(fn.low) + ") * 0.5;" :\
    fSource += variable + "+= total / maxAmplitude;")
    
    for (auto& fn : funcs.funcs) {
        switch (fn.func) {
            case TerrainFunction::RIDGED_NOISE:
                fSource += R"(
                total = 0.0;
                amplitude = 1.0;
                maxAmplitude = 0.0;
                frequency = )" + TS(fn.frequency) + R"(;

                for (int i = 0; i < )" + TS(fn.octaves) + R"(; i++) {
                    total += snoise(pos * frequency) * amplitude;

                    frequency *= 2.0;
                    maxAmplitude += amplitude;
                    amplitude *= )" + TS(fn.persistence) + R"(;
                }
                )";
                SCALE_CODE;
                break;
            default:
                break;
        }
    }
}
