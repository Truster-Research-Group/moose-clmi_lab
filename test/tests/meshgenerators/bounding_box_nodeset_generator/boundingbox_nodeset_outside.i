[Mesh]
  [./gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
    ny = 2
    parallel_type = replicated
  []

  [bounding]
    type = BoundingBoxNodeSetGenerator
    input = gmg
    new_boundary = middle_node
    top_right = '1.1 1.1 0'
    bottom_left = '0.51 0.51 0'
    location = OUTSIDE
  []
  [delete]
    type = BlockDeletionGenerator
    input = bounding
  []
[]

[Outputs]
  exodus = true
[]
