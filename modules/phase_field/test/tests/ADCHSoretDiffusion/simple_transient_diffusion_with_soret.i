[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 10
[]

[Variables]
  [./c]
  [../]
  [./mu]
  [../]
[]

[AuxVariables]
  [./T]
    [./InitialCondition]
      type = RampIC
      value_left = 900
      value_right = 1000
    [../]
  [../]
[]

[Kernels]
  [./conc]
    type = ADCHSplitConcentration
    variable = c
    chemical_potential_var = mu
    mobility = chemical_mobility_prop
  [../]
  [./chempot]
    type = ADCHSplitChemicalPotential
    variable = mu
    chemical_potential = mu_prop
  [../]
  [./soret]
    type = ADCHSoretMobility
    variable = c
    T = T
    mobility = thermal_mobility_prop
  [../]
  [./time]
    type = ADTimeDerivative
    variable = c
  [../]
[]

[Materials]
  [./chemical_potential]
    type = ADPiecewiseLinearInterpolationMaterial
    property = mu_prop
    variable = c
    x = '0 1'
    y = '0 1'
  [../]
  [./chemical_mobility_prop]
    type = GenericConstantMaterial
    prop_names = chemical_mobility_prop
    prop_values = 0.1
  [../]
  [./thermal_mobility_prop]
    type = GenericConstantMaterial
    prop_names = thermal_mobility_prop
    prop_values = -20
  [../]
[]

[BCs]
  [./leftc]
    type = DirichletBC
    variable = c
    boundary = left
    value = 0
  [../]
  [./rightc]
    type = DirichletBC
    variable = c
    boundary = right
    value = 1
  [../]
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -ksp_grmres_restart -sub_ksp_type -sub_pc_type -pc_asm_overlap'
  petsc_options_value = 'asm      31                  preonly       lu           2'
  dt = 0.1
  num_steps = 20
[]

[Preconditioning]
  [./smp]
     type = SMP
     full = true
  [../]
[]

[Outputs]
  exodus = true
[]
