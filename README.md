# QuarkLangLibs-Regex

QuarkLang 官方库 **regex** —— 正则表达式引擎，**纯 qk 实现、零依赖、跨系统**（不调用任何原生库）。

- 编译式：模式先编译为线性指令序列，再由回溯虚拟机执行（支持捕获与最左匹配）
- 可选优化：字面快速路径、入口字面前缀跳跃、首字符过滤、同类循环起点加速、零分配撤销日志
- 无效模式不抛异常：`isValid()` 返回 false，匹配一律失败

## 安装

**qkm（推荐）**：在 `cup.json` 里声明依赖后 `qkm install`

```json
{
  "dependencies": [
    { "name": "regex", "version": "1.0.0", "url": "github://QuarkLangCommunity/QuarkLangLibs-Regex@v1.0.0", "files": ["regex.qk"] }
  ]
}
```

**手工**：把 `regex.qk` 放进程序同目录（`import "regex";` 会解析同目录同名 `.qk`）。

## 快速开始

```qk
program main;
import "regex";

fn main(io IOStream) {
    io.println(regex::find("\\d+", "id=42"));                       // 42
    io.println(regex::replace("(\\w+)@(\\w+)", "u@h", "$2/$1"));   // h/u
    io.println(regex::findAll("[一-龥]+", "abc中文def").size().toString()); // 1

    re Regex = regex::compile("^\\s*(\\w+)\\s*=\\s*(.*)$");    // 热路径：编译一次复用
    g List<String> = re.findGroups("  key = value ");
    io.println(g[1] + " -> " + g[2]);
}
```

## API

### space 函数（`regex::` 前缀）

| 函数 | 说明 |
|---|---|
| `isValid(pat) bool` | 模式是否合法 |
| `compile(pat) Regex` | 编译为可复用对象 |
| `isMatch(pat, s) bool` | **锚定串首**匹配（同 Python `re.match`） |
| `fullMatch(pat, s) bool` | 全串匹配（同 Python `re.fullmatch`） |
| `search(pat, s) bool` | 任意位置是否存在匹配 |
| `find(pat, s) String` | 首个匹配文本（无 → `""`） |
| `findAll(pat, s) List<String>` | 全部非重叠匹配（含零宽，按 rune 前进） |
| `groups(pat, s) List<String>` | `[整体, 组1, 组2, …]`，未参与匹配的组为 `""` |
| `replace(pat, s, repl) String` | 全部替换（同 Python `re.sub`） |
| `split(pat, s) List<String>` | 按匹配切分 |
| `escape(s) String` | 转义元字符（拼模式用） |

> 每次调用都会重新编译模式。循环/热路径请先 `compile`。

### Regex 对象

| 方法 | 说明 |
|---|---|
| `isMatch(s)` / `fullMatch(s)` / `hasMatch(s)` | 锚定首 / 全串 / 任意位置 |
| `indexOf(s) int` | 匹配起始 **rune 下标**（无 → -1） |
| `find(s)` / `findAll(s)` / `findGroups(s)` | 同 space 版 |
| `replace(s, repl)` / `replaceFirst(s, repl)` | 全部 / 首个替换 |
| `split(s)` | 切分 |
| `groupCount() int` / `isValid() bool` / `error() String` / `source() String` | 元信息 |
| `disasm() String` | 反汇编（诊断/教学） |

替换串支持 `$0`…`$9` 组引用与 `$$`（字面 `$`）；`\1` 形式的组引用同样识别。

## 语法支持

| 语法 | 例子 | 说明 |
|---|---|---|
| 字面量 | `abc` | 整个模式无元字符时走子串快速路径 |
| 任意字符 | `.` | 除换行外任意字符（rune 级） |
| 转义类 | `\d \D \w \W \s \S` | `\s` = `\t`–`\r` 与空格；**均为 ASCII 语义** |
| 转义字符 | `\n \t \r \\. \\* ` … | 元字符转义；未知转义按字面量 |
| 字符类 | `[abc]` `[^0-9]` `[a-z0-9_]` `[一-龥]` | 支持区间、取反、\d/\w/\s 内嵌、首个 `]` 字面量 |
| 量词 | `* + ? {m} {m,} {m,n}` | 支持惰性后缀 `?`（如 `.*?`） |
| 分组 | `(…)` `(?:…)` | 捕获 / 非捕获，可嵌套；量词作用于组时捕获为最后一次迭代 |
| 交替 | `cat|dog` | 右结合，可与分组/量词组合 |
| 锚点 | `^ $` | 串首 / 串尾 |
| 词边界 | `\b \B` | 基于 ASCII 词字符 |

### 不支持（会置 `isValid()=false` 或按字面量处理）

- 反向引用 `\1`
- 前后瞻 `(?=)` `(?!) (?<=) (?<!) `
- 内联标志 `(?i)`、多行模式（`^ $` 只匹配串首尾）
- 命名分组 `(?<name>)`、条件、递归
- 类内取反类 `[\D]`（请写 `[^0-9]`）
- 灾难性回溯无步数上限（嵌套量词 + 长串请自行控制，如 `(a+)+b`）

## 性能（实测，解释器路径、单线程）

| 场景 | 耗时 |
|---|---|
| 20,000 × `compile("[0-9]+")` | 1.4 s |
| 20,000 × 预编译 `re.find("id=12345;")` | 0.85 s |
| 66,890 字符 `findAll("\\w+")` → 16,000 匹配 | 4.5 s |
| 66,890 字符 `findAll("(\\w+)@(\\w+)\\.(\\w+)")` → 4,000 匹配 | 7.6 s |

优化手段：

1. **字面快速路径**：模式不含元字符时直接子串扫描，不进虚拟机；
2. **入口字面前缀跳跃**：如 `abc.*` 只在 `abc` 出现处尝试；
3. **首字符过滤集**：如 `[0-9]+` 跳过非数字起点；
4. **同类循环起点加速**：`\w+…` 这类模式跳过"词中"起点（不影响最左匹配语义）；
5. **零分配回溯**：无捕获组时不做任何回滚；有捕获组时用撤销日志（容量复用）替代每分支快照。

## 测试

```sh
QUARK=/path/to/quark ./tests/run.sh      # 输出 failures=0 即全部通过
```

覆盖：字面量/锚点/字符类/量词（含惰性、{m,n}）/分组/交替/词边界/零宽匹配/Unicode 区间/替换组引用/切分/无效模式/起点加速对抗用例（`a$`、`\Bfoo`、`a+?`、`.*?b` 等）。

## 已知限制

- **库级无模块级变量**（语言当前仅允许 `space` 内声明函数），因此无法内置"模式缓存"；请用 `compile` 复用。
- **qkc 编译路径暂不支持** `struct`/`impl`/`space` 与 qk 库导入，本库与 cleg/json 等库一样运行在解释器路径。
- `\w \d \s` 与 `\b` 为 ASCII 语义；Unicode 文本请用显式区间（如 `[一-龥]`）。
