#include "StageRailManager.h"
#include "Camera.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include <numbers>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

void StageRailManager::InitializeRail(const LevelData* levelData) {
  railPoints_.clear();
  if (levelData) {
    for (const auto& obj : levelData->objects) {
      if (obj.type == "CURVE" && obj.name == "EventRail") {
        railPoints_ = obj.bezierPoints;
        break;
      }
    }
  }
  if (railPoints_.empty()) {
    LevelData::BezierControlPoint pt0, pt1;
    pt0.co = { 0.0f, 2.0f, -15.0f };
    pt0.handleLeft = pt0.co;
    pt0.handleRight = pt0.co;

    pt1.co = { 0.0f, 2.0f, 175.0f };
    pt1.handleLeft = pt1.co;
    pt1.handleRight = pt1.co;

    railPoints_.push_back(pt0);
    railPoints_.push_back(pt1);
  }

  if (levelData && !levelData->players.empty()) {
    maxProgress_ = levelData->players[0].distance;
    if (maxProgress_ <= 0.0f) {
      maxProgress_ = 190.0f;
    }
  } else {
    maxProgress_ = 190.0f;
  }

  cameraProgress_ = 0.0f;
  isMovementPaused_ = false;
}

void StageRailManager::InitializeTerrain(Camera* camera, const Vector3& transitionEdgeColor) {
  TextureManager::GetInstance()->LoadTexture(kTilePath);

  ModelManager::GetInstance()->LoadModel(kFloorModelDirectory, kFloorModel);
  ModelManager::GetInstance()->LoadModel(kEscalatorModelDirectory, kEscalatorModel);
  ModelManager::GetInstance()->LoadModel(kHillModelDirectory, kHillModel);

  floorObject_ = std::make_unique<Object3d>();
  floorObject_->Initialize();
  floorObject_->SetModel(kFloorModel);
  floorObject_->SetOverrideTexturePath(kTilePath);
  floorObject_->SetUvScale({ floorSettings_.uvScale.x, floorSettings_.uvScale.y });
  floorObject_->SetTranslate(floorSettings_.position);
  floorSettings_.rotation = { 0.0f, std::numbers::pi_v<float>, 0.0f };
  floorObject_->SetRotation(floorSettings_.rotation);
  floorObject_->SetScale(floorSettings_.scale);
  floorObject_->SetCamera(camera);
  floorObject_->SetDissolveMaskTexture("masks/noise0.png");
  floorObject_->SetDissolveParams(1, 1.0f, 0.05f, transitionEdgeColor);

  escalatorObject_ = std::make_unique<Object3d>();
  escalatorObject_->Initialize();
  escalatorObject_->SetModel(kEscalatorModel);
  escalatorObject_->SetTranslate(escalatorSettings_.position);
  escalatorSettings_.rotation = { 0.0f, std::numbers::pi_v<float>, 0.0f };
  escalatorObject_->SetRotation(escalatorSettings_.rotation);
  escalatorObject_->SetScale(escalatorSettings_.scale);
  escalatorObject_->SetCamera(camera);
  escalatorObject_->SetDissolveMaskTexture("masks/noise0.png");
  escalatorObject_->SetDissolveParams(1, 1.0f, 0.05f, transitionEdgeColor);

  hillObject_ = std::make_unique<Object3d>();
  hillObject_->Initialize();
  hillObject_->SetModel(kHillModel);
  hillObject_->SetTranslate(hillSettings_.position);
  hillSettings_.rotation = { 0.0f, std::numbers::pi_v<float>, 0.0f };
  hillObject_->SetRotation(hillSettings_.rotation);
  hillObject_->SetScale(hillSettings_.scale);
  hillObject_->SetCamera(camera);
  hillObject_->SetDissolveMaskTexture("masks/noise0.png");
  hillObject_->SetDissolveParams(1, 1.0f, 0.05f, transitionEdgeColor);
}

void StageRailManager::UpdateTerrain(uint32_t viewIndex, Camera* camera) {
  if (floorObject_) {
    floorObject_->Update(viewIndex, camera);
  }
  if (escalatorObject_) {
    escalatorObject_->Update(viewIndex, camera);
  }
  if (hillObject_) {
    hillObject_->Update(viewIndex, camera);
  }
}

void StageRailManager::ClearTerrainDissolve(const Vector3& transitionEdgeColor) {
  if (floorObject_) {
    floorObject_->SetDissolveParams(0, 0.0f, 0.05f, transitionEdgeColor);
  }
  if (escalatorObject_) {
    escalatorObject_->SetDissolveParams(0, 0.0f, 0.05f, transitionEdgeColor);
  }
  if (hillObject_) {
    hillObject_->SetDissolveParams(0, 0.0f, 0.05f, transitionEdgeColor);
  }
}

void StageRailManager::DrawTerrain() {
  if (floorObject_) floorObject_->Draw(0);
  if (escalatorObject_) escalatorObject_->Draw(0);
  if (hillObject_) hillObject_->Draw(0);
}

void StageRailManager::ForEachTerrainObject(const std::function<void(Object3d*)>& func) {
  if (floorObject_) func(floorObject_.get());
  if (escalatorObject_) func(escalatorObject_.get());
  if (hillObject_) func(hillObject_.get());
}

void StageRailManager::Finalize() {
  floorObject_.reset();
  escalatorObject_.reset();
  hillObject_.reset();
}

#ifdef USE_IMGUI
void StageRailManager::UpdateImGui() {
  if (ImGui::TreeNode("Terrain Objects")) {
    if (floorObject_ && ImGui::TreeNode("Floor Object")) {
      Vector3 floorPos = floorObject_->GetTranslate();
      if (ImGui::DragFloat3("translate", &floorPos.x, 0.1f)) {
        floorObject_->SetTranslate(floorPos);
      }
      Vector3 floorRot = floorObject_->GetRotation();
      if (ImGui::DragFloat3("rotate", &floorRot.x, 0.01f)) {
        floorObject_->SetRotation(floorRot);
      }
      Vector3 floorScale = floorObject_->GetScale();
      if (ImGui::DragFloat3("scale", &floorScale.x, 0.1f)) {
        floorObject_->SetScale(floorScale);
      }
      ImGui::DragFloat2("UV Scale", &floorSettings_.uvScale.x, 0.1f);
      floorObject_->SetUvScale(floorSettings_.uvScale);
      ImGui::TreePop();
    }
    if (escalatorObject_ && ImGui::TreeNode("Escalator Object")) {
      Vector3 escPos = escalatorObject_->GetTranslate();
      if (ImGui::DragFloat3("translate", &escPos.x, 0.1f)) {
        escalatorObject_->SetTranslate(escPos);
      }
      Vector3 escRot = escalatorObject_->GetRotation();
      if (ImGui::DragFloat3("rotate", &escRot.x, 0.01f)) {
        escalatorObject_->SetRotation(escRot);
      }
      Vector3 escScale = escalatorObject_->GetScale();
      if (ImGui::DragFloat3("scale", &escScale.x, 0.1f)) {
        escalatorObject_->SetScale(escScale);
      }
      ImGui::TreePop();
    }
    if (hillObject_ && ImGui::TreeNode("Hill Object")) {
      Vector3 hillPos = hillObject_->GetTranslate();
      if (ImGui::DragFloat3("translate", &hillPos.x, 0.1f)) {
        hillObject_->SetTranslate(hillPos);
      }
      Vector3 hillRot = hillObject_->GetRotation();
      if (ImGui::DragFloat3("rotate", &hillRot.x, 0.01f)) {
        hillObject_->SetRotation(hillRot);
      }
      Vector3 hillScale = hillObject_->GetScale();
      if (ImGui::DragFloat3("scale", &hillScale.x, 0.1f)) {
        hillObject_->SetScale(hillScale);
      }
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }
}
#endif

Vector3 StageRailManager::CalculateRailPosition(float progress) const {
  if (railPoints_.empty()) {
    return { 0.0f, 2.0f, -15.0f };
  }

  float p = progress / maxProgress_;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;

  size_t N = railPoints_.size() - 1;
  float scaledP = p * static_cast<float>(N);
  size_t i = static_cast<size_t>(scaledP);
  if (i >= N) {
    i = N - 1;
  }
  float t = scaledP - static_cast<float>(i);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  const auto& p0 = railPoints_[i].co;
  const auto& p1 = railPoints_[i].handleRight;
  const auto& p2 = railPoints_[i + 1].handleLeft;
  const auto& p3 = railPoints_[i + 1].co;

  float u = 1.0f - t;
  float tt = t * t;
  float uu = u * u;
  float uuu = uu * u;
  float ttt = tt * t;

  Vector3 pos;
  pos.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
  pos.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
  pos.z = uuu * p0.z + 3.0f * uu * t * p1.z + 3.0f * u * tt * p2.z + ttt * p3.z;
  return pos;
}

Vector3 StageRailManager::GetCameraRailPosition(float progress, float playerEyeHeight) const {
  Vector3 pos = CalculateRailPosition(progress);
  pos.y += playerEyeHeight;
  return pos;
}
