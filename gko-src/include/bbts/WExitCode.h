/**
 * @file WExitCode.h
 *
 * @author liuming03
 * @date 2013-6-26
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_WEXITCODE_H_
#define OP_OPED_NOAH_TOOLS_BBTS_WEXITCODE_H_

#include <sys/wait.h>

#include <string>

namespace bbts {

/**
 * @brief wait/waitpid/system等返回值转换
 */
class WExitCode {
 public:
  enum exit_type {
    T_WUNKNOW,
    T_WEXITED,
    T_WSIGNALED,
    T_WSTOPPED,
    T_WCONTINUED,
  };

  WExitCode(int status) :
      _type(T_WUNKNOW), _code(status) {
    if (WIFEXITED(status)) {
      _type = T_WEXITED;
      _code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      _type = T_WSIGNALED;
      _code = WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
      _type = T_WSTOPPED;
      _code = WSTOPSIG(status);
    }
  }

  inline int get_code() {
    return _code;
  }

  inline exit_type get_type() {
    return _type;
  }

  inline std::string get_type_string() {
    static std::string EXIT_TYPE_STRING[] = {
        "UNKNOW",
        "EXITED",
        "SIGNALED",
        "STOPPED",
        "CONTINUED",
    };
    return EXIT_TYPE_STRING[_type];
  }

 private:
  exit_type _type;
  int _code;

};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_WEXITCODE_H_
