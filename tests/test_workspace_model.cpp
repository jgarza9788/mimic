#include "mimic/MimicWorkspaceModel.hh"

MIMIC_TEST(workspace_model_always_starts_with_one_workspace) {
  mimic::MimicWorkspaceModel model;
  MIMIC_ASSERT(model.workspace_count() == 1);
}

MIMIC_TEST(workspace_model_removes_empty_non_last_workspace) {
  mimic::MimicWorkspaceModel model;
  model.create_workspace();

  const bool removed = model.maybe_remove_empty_workspace(1);
  MIMIC_ASSERT(removed);
  MIMIC_ASSERT(model.workspace_count() == 1);
}

MIMIC_TEST(workspace_model_never_removes_last_workspace) {
  mimic::MimicWorkspaceModel model;
  const bool removed = model.maybe_remove_empty_workspace(0);
  MIMIC_ASSERT(!removed);
  MIMIC_ASSERT(model.workspace_count() == 1);
}
