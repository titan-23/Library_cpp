import optuna
import time
import os
import multiprocessing
from parallel_tester import ParallelTester, build_tester
from ahc_settings import AHCSettings

def output(output_dir: str, study: optuna.Study) -> None:
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(f'{output_dir}/result.txt', 'w', encoding='utf-8') as f:
    print(study.best_trial, file=f)

  fig = optuna.visualization.plot_contour(study)
  fig.write_html(f'{output_dir}/contour.html')
  fig.write_image(f'{output_dir}/contour.png')
  fig = optuna.visualization.plot_edf(study)
  fig.write_html(f'{output_dir}/edf.html')
  fig.write_image(f'{output_dir}/edf.png')
  fig = optuna.visualization.plot_optimization_history(study)
  fig.write_html(f'{output_dir}/optimization_history.html')
  fig.write_image(f'{output_dir}/optimization_history.png')
  fig = optuna.visualization.plot_parallel_coordinate(study)
  fig.write_html(f'{output_dir}/parallel_coordinate.html')
  fig.write_image(f'{output_dir}/parallel_coordinate.png')
  fig = optuna.visualization.plot_slice(study)
  fig.write_html(f'{output_dir}/slice.html')
  fig.write_image(f'{output_dir}/slice.png')

if __name__ == "__main__":
  tester: ParallelTester = build_tester(AHCSettings.n_jobs_multi_run)
  tester.compile()

  start = time.time()

  study: optuna.Study = optuna.create_study(
    direction=AHCSettings.direction,
    study_name=AHCSettings.study_name,
    storage=f'sqlite:///{AHCSettings.study_name}.db',
    load_if_exists=True,
  )

  def _objective(trial: optuna.trial.Trial):
    tester: ParallelTester = build_tester(AHCSettings.n_jobs_multi_run)
    args = AHCSettings.objective(trial)
    tester.append_execute_command(args)
    scores = tester.run()
    score = tester.get_score(scores)
    return score

  study.optimize(
    _objective,
    n_trials=AHCSettings.n_trials,
    n_jobs=min(AHCSettings.n_jobs_optuna, multiprocessing.cpu_count()-1),
  )

  print(study.best_trial)
  print('write image ...')
  output(f'./imgae_{AHCSettings.study_name}', study)
  print(f'Finish parameter seraching. Time: {time.time() - start}sec.')
