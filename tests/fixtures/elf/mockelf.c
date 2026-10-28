/* W2UCEM symtab 校验 fixture：小型真实 ELF。
 * 结构刻意覆盖规范化器的关键维度：
 *   - .dynsym：导出 mock_export、导入 puts/printf（UND）
 *   - .symtab：LOCAL 内部符号、HIDDEN 可见性、WEAK 绑定
 * 由 tests/fixtures/elf/gen_elf.sh 在 Linux 上重新生成并自检。 */
#include <stdio.h>

static void internal_helper(void) {
  printf("helper\n");
}

int mock_export(void) {
  internal_helper();
  puts("mock export");
  return 0;
}

int main(void) {
  return mock_export();
}
