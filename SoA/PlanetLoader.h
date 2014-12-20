///
/// PlanetLoader.h
/// Seed of Andromeda
///
/// Created by Benjamin Arnold on 19 Dec 2014
/// Copyright 2014 Regrowth Studios
/// All Rights Reserved
///
/// Summary:
/// Handles the loading of planet files and the
/// construction of GPU generation shaders.
///

#pragma once

#ifndef PlanetLoader_h__
#define PlanetLoader_h__

#pragma once

#include <GLProgram.h>
#include <Keg.h>
#include <vector>

class IOManager;
class TerrainFuncs;

class PlanetGenData {
public:
    VGTexture surfaceColorMap;
    VGTexture watercolorMap;
    std::vector<VGTexture> biomeMaps;
    vg::GLProgram* program;
};

class PlanetLoader {
public:
    PlanetLoader(IOManager* ioManager);
    ~PlanetLoader();

    PlanetGenData* loadPlanet(const nString& filePath);
    PlanetGenData* getDefaultGenData();
private:
    void parseTerrainFuncs(TerrainFuncs* terrainFuncs, YAML::Node& node);
    vg::GLProgram* generateProgram(TerrainFuncs& baseTerrainFuncs,
                                   TerrainFuncs& tempTerrainFuncs,
                                   TerrainFuncs& humTerrainFuncs);
    void addNoiseFunctions(nString& fSource, const nString& variable, const TerrainFuncs& funcs);

    PlanetGenData* m_defaultGenData = nullptr;

    IOManager* m_iom = nullptr;
};

#endif // PlanetLoader_h__

