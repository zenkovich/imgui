#pragma once

#include <vector>

#include "cg_Config.h"
#include "cg_Graph.h"

namespace cg {

class Physics {
 public:
  Physics(const Config& cfg, Graph& graph);
  void Step();
  // Profiling breakdown helpers
  void IntegrateOnly();
  void ConstraintsOnly();
  void RepulsionOnly();
  void AngleEqualizationOnly();

 private:
  void Integrate();
  void SatisfyConstraints();
  void ApplyRepulsion();
  void EqualizeChildAngles();
  void InitializeDirectoryMasses();
  int ComputeDescendantCount(int nodeId, std::vector<int>& memo);

  const Config& _config;
  Graph& _graph;
};

}  // namespace cg


