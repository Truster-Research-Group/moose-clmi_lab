T_hs = 300
T_ambient = 500
htc = 100
t = 0.001

L = 2
D_i = 0.2
thickness = 0.5

# SS 316
density = 8.0272e3
specific_heat_capacity = 502.1
conductivity = 16.26

R_i = ${fparse 0.5 * D_i}
D_o = ${fparse D_i + 2 * thickness}
A = ${fparse pi * D_o * L}
heat_flux = ${fparse htc * (T_ambient - T_hs)}
E_change = ${fparse heat_flux * A * t}

[HeatStructureMaterials]
  [./hs_mat]
    type = SolidMaterialProperties
    rho = ${density}
    Cp = ${specific_heat_capacity}
    k = ${conductivity}
  [../]
[]

[Components]
  [./hs]
    type = HeatStructureCylindrical
    orientation = '0 0 1'
    position = '0 0 0'
    length = ${L}
    n_elems = 10

    inner_radius = ${R_i}
    widths = '${thickness}'
    n_part_elems = '10'
    materials = 'hs_mat'
    names = 'region'

    initial_T = ${T_hs}
  [../]

  [./ambient_convection]
    type = HSBoundaryAmbientConvection
    hs = hs
    hs_side = top
    T_ambient = ${T_ambient}
    htc_ambient = ${htc}
  [../]
[]

[Postprocessors]
  [./E_hs]
    type = HeatStructureEnergyRZ
    block = 'hs:region'
    axis_dir = '0 0 1'
    axis_point = '0 0 0'
    execute_on = 'INITIAL TIMESTEP_END'
  [../]
  [./E_hs_change]
    type = ChangeOverTimePostprocessor
    postprocessor = E_hs
    execute_on = 'INITIAL TIMESTEP_END'
  [../]
  [./E_change_relerr]
    type = RelativeDifferencePostprocessor
    value1 = E_hs_change
    value2 = ${E_change}
    execute_on = 'INITIAL TIMESTEP_END'
  [../]
[]

[Executioner]
  type = Transient

  [./TimeIntegrator]
    type = ActuallyExplicitEuler
    solve_type = lumped
  [../]
  dt = ${t}
  num_steps = 1
  abort_on_solve_fail = true
[]

[Outputs]
  [./out]
    type = CSV
    show = 'E_change_relerr'
    execute_on = 'FINAL'
  [../]
[]
