//
// myshell.c : 簡易UNIXシェル（リダイレクト機能準備完了版）
//
#include <stdio.h>                              // perror() のため
#include <stdlib.h>                             // exit() のため
#include <string.h>                             // strcmp(), strchr() のため
#include <unistd.h>                             // fork(), exec(), close() のため
#include <sys/wait.h>                           // wait() のため
#include <ctype.h>                              // isspace() のため
#include <fcntl.h>                              // open() のため

#define MAXLINE 1000                            // コマンド行の最大文字数
#define MAXARGS 60                              // コマンド行文字列の最大数

int parse(char *p, char *args[]) {              // コマンド行を解析する
  int i = 0;                                    // 解析後文字列の数
  for (;;) {
    while (isspace(*p)) *p++ = '\0';            // 空白を'\0'に書換える
    if (*p == '\0' || i >= MAXARGS) break;      // コマンド行の終端に到達で終了
    args[i++] = p;                              // 文字列を文字列配列に記録
    while (*p != '\0' && !isspace(*p)) p++;     // 文字列の最後まで進む
  }
  args[i] = NULL;                               // 文字列配列の終端マーク
  return *p == '\0';                            // 解析完了なら 1 を返す
}

void cdCom(char *args[]) {                      // cd コマンドを実行する
  if (args[1] == NULL || args[2] != NULL) {     // 引数を確認して
    fprintf(stderr, "Usage: cd DIR\n");         // 過不足ありなら使い方表示
  } else if (chdir(args[1]) < 0) {              // 親プロセスが chdir する
    perror(args[1]);                            // chdir に失敗したら perror
  }
}

void setenvCom(char *args[]) {                  // setenv コマンドを実行する
  if (args[1] == NULL || args[2] == NULL || args[3] != NULL) {   // 引数を確認して
    fprintf(stderr, "Usage: setenv NAME VAL\n");// 過不足ありなら使い方表示
  } else if (setenv(args[1], args[2], 1) < 0) { // 親プロセスが setenv する
    perror(args[1]);                            // setenv に失敗したら perror
  }
}

void unsetenvCom(char *args[]) {                // unsetenv コマンドを実行する
  if (args[1] == NULL || args[2] != NULL) {     // 引数を確認して
    fprintf(stderr, "Usage: unsetenv NAME\n");  // 過不足ありなら使い方表示
  } else if (unsetenv(args[1]) < 0) {           // 親プロセスが unsetenv する
    perror(args[1]);                            // unsetenv に失敗なら perror
  }
}

char *ofile;                                    // 出力リダイレクトファイル名
char *ifile;                                    // 入力リダイレクトファイル名

void findRedirect(char *args[]) {               // リダイレクトの指示を探す
  int i, j;
  ofile = ifile = NULL;
  for (i = j = 0; args[i] != NULL; i++) {       // コマンド行の全文字列について
    if (strcmp(args[i], "<") == 0) {            // 入力リダイレクト発見
      ifile = args[++i];                        // ファイル名を記録する
      if (ifile == NULL) break;                 // ファイル名が無かった
    } else if (strcmp(args[i], ">") == 0) {     // 出力リダイレクト発見
      ofile = args[++i];                        // ファイル名を記録する
      if (ofile == NULL) break;                 // ファイル名が無かった
    } else {                                    // どちらでもない
      args[j++] = args[i];                      // 文字列を args に記録する
    }
  }
  args[j] = NULL;
}

void redirect(int fd, char *path, int flag) {
  int new_fd = open(path, flag, 0666);          // リダイレクト先のファイルを開く
  if (new_fd < 0) {                             // ファイルが開けなかった場合
    perror(path);                               // エラーメッセージを表示し
    exit(1);                                    // 子プロセスを終了する
  }
  if (dup2(new_fd, fd) < 0) {                   // ファイルディスクリプタを複製する
    perror("dup2");                             // 失敗した場合
    exit(1);                                    // 子プロセスを終了する
  }
  close(new_fd);                                // 複製後のファイルディスクリプタを閉じる
}

void externalCom(char *args[]) {
  int pid, status;
  if ((pid = fork()) < 0) {
    perror("fork");
    exit(1);
  }
  if (pid == 0) {                               // 子プロセス
    if (ifile != NULL) {                        // 入力リダイレクトがある場合
      redirect(STDIN_FILENO, ifile, O_RDONLY);  // 標準入力をリダイレクト
    }
    if (ofile != NULL) {                        // 出力リダイレクトがある場合
      redirect(STDOUT_FILENO, ofile, O_WRONLY | O_TRUNC | O_CREAT); // 標準出力をリダイレクト
    }
    execvp(args[0], args);                      // コマンドを実行
    perror(args[0]);                            // execvp が失敗した場合
    exit(1);
  } else {                                      // 親プロセス
    while (wait(&status) != pid)                // 子プロセスの終了を待つ
      ;
  }
}

void execute(char *args[]) {
  if (strcmp(args[0], "cd") == 0) {             // cd コマンド
    cdCom(args);
  } else if (strcmp(args[0], "setenv") == 0) {  // setenv コマンド
    setenvCom(args);
  } else if (strcmp(args[0], "unsetenv") == 0) {// unsetenv コマンド
    unsetenvCom(args);
  } else {                                      // その他は外部コマンド
    externalCom(args);
  }
}

int main() {
  char buf[MAXLINE+2];                          // コマンド行を格納する配列
  char *args[MAXARGS+1];                        // 解析結果を格納する文字列配列
  for (;;) {
    printf("Command: ");                        // プロンプトを表示する
    if (fgets(buf, MAXLINE+2, stdin) == NULL) { // コマンド行を入力する
      printf("\n");                             // EOF なら
      break;                                    // 正常終了する
    }
    if (strchr(buf, '\n') == NULL) {            // '\n' がバッファにない場合は
      fprintf(stderr, "行が長すぎる\n");        // コマンド行が長すぎたので
      return 1;                                 // 異常終了する
    }
    if (!parse(buf, args)) {                    // コマンド行を解析する
      fprintf(stderr, "引数が多すぎる\n");      // 文字列が多すぎる場合は
      continue;                                 // ループの先頭に戻る
    }
    findRedirect(args);                         // リダイレクトの指示を見つける
    if (args[0] != NULL) execute(args);         // コマンドを実行する
  }
  return 0;
}
