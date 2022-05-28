#include "MaterialGenerator.h"

#include "../StringUtils.h"
#include "../materials/RenderMode.h"

MaterialGenerator::MaterialGenerator(const DisplayListSettings& settings): mSettings(settings) {}


bool MaterialGenerator::ShouldIncludeNode(aiNode* node) {
    return false;
}

int sortOrderForMaterial(const Material& material) {
    // assume opaque
    if (!material.mState.hasRenderMode) {
        return 0;
    }

    if (material.mState.cycle1RenderMode.GetZMode() == ZMODE_DEC) {
        return 1;
    }

    if (material.mState.cycle1RenderMode.data & FORCE_BL) {
        return 2;
    }

    return 0;
}

void MaterialGenerator::GenerateDefinitions(const aiScene* scene, CFileDefinition& fileDefinition) {
    std::set<std::shared_ptr<TextureDefinition>> textures;

    for (auto& entry : mSettings.mMaterials) {
        for (int i = 0; i < 8; ++i) {
            if (entry.second->mState.tiles[i].texture) {
                textures.insert(entry.second->mState.tiles[i].texture);
            }
        }
    }

    for (auto& texture : textures) {
        fileDefinition.AddDefinition(std::move(texture->GenerateDefinition(fileDefinition.GetUniqueName(texture->Name()), "_mat")));
    }
    
    int index = 0;

    std::unique_ptr<StructureDataChunk> materialList(new StructureDataChunk());
    std::unique_ptr<StructureDataChunk> revertList(new StructureDataChunk());

    std::vector<std::shared_ptr<Material>> materialsAsVector;

    for (auto& entry : mSettings.mMaterials) {
        materialsAsVector.push_back(entry.second);
    }

    std::sort(materialsAsVector.begin(), materialsAsVector.end(), [&](const std::shared_ptr<Material>& a, const std::shared_ptr<Material>& b) -> bool {
        return sortOrderForMaterial(*a) < sortOrderForMaterial(*b);
    });

    for (auto& entry : materialsAsVector) {
        std::string name = fileDefinition.GetUniqueName(entry->mName);

        DisplayList dl(name);
        entry->Write(fileDefinition, mSettings.mDefaultMaterialState, dl.GetDataChunk());
        std::unique_ptr<FileDefinition> material = dl.Generate("_mat");
        materialList->AddPrimitive(material->GetName());
        fileDefinition.AddDefinition(std::move(material));

        std::string revertName = fileDefinition.GetUniqueName(entry->mName + "_revert");
        DisplayList revertDL(revertName);
        generateMaterial(fileDefinition, entry->mState, mSettings.mDefaultMaterialState, revertDL.GetDataChunk());
        std::unique_ptr<FileDefinition> materialRevert = revertDL.Generate("_mat");
        revertList->AddPrimitive(materialRevert->GetName());
        fileDefinition.AddDefinition(std::move(materialRevert));

        fileDefinition.AddMacro(MaterialIndexMacroName(entry->mName), std::to_string(index));

        ++index;
    }

    unsigned transparentIndex = 0;

    while (transparentIndex < materialsAsVector.size() && sortOrderForMaterial(*materialsAsVector[transparentIndex]) == 0) {
        ++transparentIndex;
    }

    fileDefinition.AddMacro(fileDefinition.GetMacroName("MATERIAL_COUNT"), std::to_string(index));
    fileDefinition.AddMacro(fileDefinition.GetMacroName("TRANSPARENT_START"), std::to_string(transparentIndex + 1));

    fileDefinition.AddDefinition(std::unique_ptr<FileDefinition>(new DataFileDefinition("Gfx*", fileDefinition.GetUniqueName("material_list"), true, "_mat", std::move(materialList))));
    fileDefinition.AddDefinition(std::unique_ptr<FileDefinition>(new DataFileDefinition("Gfx*", fileDefinition.GetUniqueName("material_revert_list"), true, "_mat", std::move(revertList))));
}

std::string MaterialGenerator::MaterialIndexMacroName(const std::string& materialName) {
    std::string result = materialName;
    std::transform(materialName.begin(), materialName.end(), result.begin(), ::toupper);
    makeCCompatible(result);
    return result + "_INDEX";
}