[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 120
  ny = 1
  xmin = 0
  xmax = 6
  ymin = 0
  ymax = 0.05
[]

[GlobalParams]
  PorousFlowDictator = dictator
[]

[UserObjects]
  [./dictator]
    type = PorousFlowDictator
    porous_flow_vars = pressure
    number_fluid_phases = 1
    number_fluid_components = 1
  [../]
  [./pc]
    type = PorousFlowCapillaryPressureVG
    m = 0.336
    alpha = 1.43e-4
  [../]
[]

[Modules]
  [./FluidProperties]
    [./simple_fluid]
      type = SimpleFluidProperties
      bulk_modulus = 2e7
      viscosity = 1.01e-3
      density0 = 1000
      thermal_expansion = 0
    [../]
  [../]
[]

[Materials]
  [./massfrac]
    type = PorousFlowMassFraction
    at_nodes = true
  [../]
  [./temperature]
    type = PorousFlowTemperature
    at_nodes = true
  [../]
  [./temperature_qp]
    type = PorousFlowTemperature
  [../]
  [./simple_fluid]
    type = PorousFlowSingleComponentFluid
    fp = simple_fluid
    phase = 0
    at_nodes = true
  [../]
  [./simple_fluid_qp]
    type = PorousFlowSingleComponentFluid
    fp = simple_fluid
    phase = 0
  [../]
  [./dens_all]
    type = PorousFlowJoiner
    include_old = true
    at_nodes = true
    material_property = PorousFlow_fluid_phase_density_nodal
  [../]
  [./dens_qp_all]
    type = PorousFlowJoiner
    material_property = PorousFlow_fluid_phase_density_qp
    at_nodes = false
  [../]
  [./visc_all]
    type = PorousFlowJoiner
    at_nodes = true
    material_property = PorousFlow_viscosity_nodal
  [../]
  [./ppss]
    type = PorousFlow1PhaseP
    at_nodes = true
    porepressure = pressure
    capillary_pressure = pc
  [../]
  [./ppss_qp]
    type = PorousFlow1PhaseP
    porepressure = pressure
    capillary_pressure = pc
  [../]
  [./relperm]
    type = PorousFlowRelativePermeabilityVG
    at_nodes = true
    m = 0.336
    seff_turnover = 0.99
    phase = 0
  [../]
  [./relperm_all]
    type = PorousFlowJoiner
    at_nodes = true
    material_property = PorousFlow_relative_permeability_nodal
  [../]
  [./porosity]
    type = PorousFlowPorosityConst
    at_nodes = true
    porosity = 0.33
  [../]
  [./permeability]
    type = PorousFlowPermeabilityConst
    permeability = '0.295E-12 0 0  0 0.295E-12 0  0 0 0.295E-12'
  [../]
[]

[Variables]
  [./pressure]
    initial_condition = 0.0
  [../]
[]

[Kernels]
  [./mass0]
    type = PorousFlowMassTimeDerivative
    fluid_component = 0
    variable = pressure
  [../]
  [./flux0]
    type = PorousFlowAdvectiveFlux
    fluid_component = 0
    variable = pressure
    gravity = '-10 0 0'
  [../]
[]

[AuxVariables]
  [./SWater]
    family = MONOMIAL
    order = CONSTANT
  [../]
[]

[AuxKernels]
  [./SWater]
    type = MaterialStdVectorAux
    property = PorousFlow_saturation_qp
    index = 0
    variable = SWater
  [../]
[]

[BCs]
  [./base]
    type = DirichletBC
    boundary = left
    value = 0.0
    variable = pressure
  [../]
[]

[Preconditioning]
  [./andy]
    type = SMP
    full = true
    petsc_options = '-snes_converged_reason -ksp_diagonal_scale -ksp_diagonal_scale_fix -ksp_gmres_modifiedgramschmidt -snes_linesearch_monitor'
    petsc_options_iname = '-ksp_type -pc_type -sub_pc_type -sub_pc_factor_shift_type -pc_asm_overlap -snes_atol -snes_rtol -snes_max_it'
    petsc_options_value = 'gmres      asm      lu           NONZERO                   2               1E-10      1E-10      10'
  [../]
[]

[VectorPostprocessors]
  [./swater]
    type = LineValueSampler
    variable = SWater
    start_point = '0 0 0'
    end_point = '6 0 0'
    sort_by = x
    num_points = 121
    execute_on = timestep_end
  [../]
[]

[Executioner]
  type = Transient
  solve_type = Newton
  petsc_options = '-snes_converged_reason'
  end_time = 345600

  [./TimeStepper]
    type = FunctionDT
    time_dt = '1E-2 1 10 500 5000 50000'
    time_t = '0 10 100 1000 10000 500000'
  [../]
[]

[Outputs]
  file_base = rd02
  [./exodus]
    type = Exodus
    execute_on = final
  [../]
  [./along_line]
    type = CSV
    execute_on = final
  [../]
[]