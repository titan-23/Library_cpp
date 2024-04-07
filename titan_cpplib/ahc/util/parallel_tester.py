from typing import Iterable, List, Callable
import argparse
import subprocess
import multiprocessing
import time
from functools import partial
from ahc_settings import AHCSettings

class ParallelTester():
  """テストケース並列回し屋です。"""

  def __init__(self,
               compile_command: str,
               execute_command: str,
               input_file_names: List[str],
               cpu_count: int,
               verbose: bool,
               get_score: Callable[[List[float]], float],
               ) -> None:
    """
    Args:
      compile_command (str): コンパイルコマンドです。
      execute_command (str): 実行コマンドです。
                             実行時引数は ``append_execute_command`` メソッドで指定することも可能です。
      input_file_names (List[str]): 入力ファイル名のリストです。
      cpu_count (int): CPU数です。
      verbose (bool): ログを表示します。
      get_score (Callable[[List[float]], float]): スコアのリストに対して平均などを取って返してください。
    """
    self.compile_command = compile_command.split()
    self.execute_command = execute_command.split()
    self.input_file_names = input_file_names
    self.cpu_count = cpu_count
    self.verbose = verbose
    self.get_score = get_score

  def show_score(self, scores: List[float]) -> float:
    """スコアのリストを受け取り、 ``get_score`` 関数で計算します。
    ついでに表示もします。
    """
    score = self.get_score(scores)
    print(f'Pred Score= {score}')
    return score

  def append_execute_command(self, args: Iterable[str]) -> None:
    """コマンドライン引数を追加します。
    """
    for arg in args:
      self.execute_command.append(str(arg))

  def compile(self) -> None:
    """``compile_command`` よりコンパイルします。
    """
    print('compiling...')
    subprocess.run(self.compile_command, stderr=subprocess.PIPE, stdout=subprocess.PIPE, text=True, check=True)

  def _process_file(self, input_file: str) -> float:
    with open(input_file, 'r', encoding='utf-8') as f:
      input_text = ''.join(f.read())
    try:
      result = subprocess.run(self.execute_command,
                              input=input_text,
                              stderr=subprocess.PIPE,
                              stdout=subprocess.PIPE,
                              text=True,
                              check=True)
      score_line = result.stderr.rstrip().split('\n')[-1]
      _, score = score_line.split(' = ')
      score = float(score)
      if self.verbose:
        print(f'{input_file}: {score=}')
      return score
    except Exception as e:
      print(e)
      print(f'Error occured in {input_file}')
      raise ValueError(input_file)

  def run(self) -> List[float]:
    """実行します。"""
    pool = multiprocessing.Pool(processes=self.cpu_count)
    scores = pool.map(partial(self._process_file), self.input_file_names, chunksize=1)
    pool.close()
    pool.join()
    return scores

  @staticmethod
  def get_args() -> argparse.Namespace:
    """実行時引数を解析します。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--compile',
                        required=False,
                        action='store_true',
                        default=False,
                        help='if compile the file. default is `False`.')
    parser.add_argument('-v', '--verbose',
                        required=False,
                        action='store_true',
                        default=False,
                        help='show logs. default is `False`.')
    parser.add_argument('-njobs', '--number_of_jobs',
                        required=False,
                        type=int,
                        action='store',
                        default=1,
                        help='set the number of cpu_count(). default is `1`.')
    return parser.parse_args()


def build_tester(njobs: int=1,
                 verbose: bool=False
                 ) -> ParallelTester:
  tester = ParallelTester(
    compile_command=AHCSettings.compile_command,
    execute_command=AHCSettings.execute_command,
    input_file_names=AHCSettings.input_file_names,
    cpu_count=min(njobs, multiprocessing.cpu_count()-1),
    verbose=verbose,
    get_score=AHCSettings.get_score,
  )
  return tester

def main():
  """実行時引数をもとに、 ``tester`` を立ち上げ実行します。"""
  args = ParallelTester.get_args()
  njobs = min(args.number_of_jobs, multiprocessing.cpu_count()-1)
  print(f'{njobs=}')

  tester = build_tester(njobs, args.verbose)

  if args.compile:
    tester.compile()

  print('start.')

  start = time.time()
  scores = tester.run()
  score = tester.show_score(scores)
  print(f'{time.time() - start:.4f}sec')
  return score

if __name__ == '__main__':
  main()