# Node2Shell (CVE-2026-111497) local RCE PoC

인증 없이 보낸 Brotli 압축 요청이 Express 본문 파서를 거쳐 취약한 Node.js 디코더에 도달하는 과정을 재현합니다. 이 PoC는 **Docker 컨테이너 내부에서 연결형 셸을 획득하는 경로**를 검증합니다.

## 환경 준비

Windows에서 Docker Desktop을 실행하고 Linux 컨테이너 엔진이 준비될 때까지 기다립니다. Python 3.10 이상도 필요합니다. 저장소 루트의 PowerShell에서 다음 명령으로 상태를 확인합니다.

~~~powershell
python --version
docker compose version
docker info --format '{{.OSType}}'
~~~

마지막 명령이 `linux`를 출력하면 대상 컨테이너를 실행할 준비가 된 것입니다. Docker 연결 오류가 나오면 Docker Desktop의 엔진이 실행 중인지 확인합니다. 컨테이너 빌드와 시작은 아래 `exp.py`가 처리하므로 `docker compose up`을 따로 실행할 필요는 없습니다.

## PoC 실행

준비를 마친 뒤 저장소 루트의 PowerShell에서 실행합니다.

~~~powershell
python .\exploit\exp.py
~~~

스크립트가 단일 대상 컨테이너를 빌드 및 시작하고, 127.0.0.1:18086의 응답을 확인한 다음 PoC를 시도합니다. 이전 Compose 구성의 잔여 컨테이너가 있으면 자동으로 제거합니다. 셸이 열리면 id, pwd 등을 입력하고 exit로 종료합니다. 성공은 매번 보장되지 않으므로 기본 50회까지 시도합니다. `--attempts`로 최대 시도 횟수, `--timeout`으로 연결 제한 시간을 조절할 수 있습니다. 옵션은 다음으로 확인할 수 있습니다.

~~~powershell
python .\exploit\exp.py -h
~~~

[>]는 진행, [+]는 성공, [-]는 실패를 뜻합니다. 처음에는 Brotli 입력 생성, ROP 체인과 `/app/bridge` 실행 경로의 입력 내 배치, Express 요청 경로를 보여주고, 이후에는 `Attempt 1/50`처럼 시도 번호만 표시합니다. 다음 번호가 나오면 앞선 시도에서 셸을 확인하지 못해 대상 Node 프로세스를 다시 시작한 것입니다. `[+] Live lab shell confirmed`는 같은 요청 연결에서 `/app/bridge`와 명령 응답을 확인했다는 뜻이며, 메모리 덮어쓰기를 직접 관찰했다는 뜻은 아닙니다. 50회 모두 실패할 수도 있습니다.

## 성공 결과와 셸 범위

성공했을 때 출력되는 메시지와 명령 결과의 일부입니다. 시도 횟수는 실행마다 다를 수 있습니다.

~~~text
[>] Preparing the local Docker target
[>] Attempt 12/50
[>] Lab helper answered; verifying a command on the same socket
[+] Live lab shell confirmed on attempt 12/50
    You are the node user inside Docker. Type exit to close the shell.
$ whoami
node
$ id
uid=1000(node) gid=1000(node) groups=1000(node)
$ exit
~~~

이 셸은 Windows 호스트가 아닌 **Docker 컨테이너 안의 `node` 계정으로 실행되는 `/bin/sh -i`**입니다. PoC가 연 HTTP 연결에 셸 입출력이 붙어 있어 연결을 닫으면 셸도 종료됩니다. TTY가 없으므로 `job control` 경고가 나올 수 있습니다. PoC 종료 후 컨테이너를 중지하려면 `docker compose down`을 실행합니다. 익스플로잇 코드와 실행 진입점은 `exploit/exp.py` 한 파일입니다.

## PoC 실행 전제

| 항목 | 이 저장소에서 미리 정해 둔 조건 |
| --- | --- |
| 실행 환경 | Docker Desktop의 Linux x86-64 컨테이너, Python 3.10 이상, 호스트의 127.0.0.1:18086 포트 |
| 취약 입력 경로 | 인증 없는 `POST /upload`가 `express.json()`을 거쳐 `Content-Encoding: br` 본문을 Node.js Brotli 디코더에 전달 |
| 고정 런타임 | 이미지 다이제스트로 고정한 Node.js 24.22.0, Express 5.3.0, `UV_THREADPOOL_SIZE=1` |
| 익스플로잇 프로필 | 이 바이너리에 맞는 메모리 주소·ROP 가젯 값과 요청 소켓 번호 19를 코드에 미리 설정. 대상의 주소나 소켓 번호를 자동 탐지하지 않음 |
| PoC 전용 셸 연결 도우미 | Docker 이미지에 `/app/bridge`가 미리 설치됨. 코드 실행이 성공하면 이 프로그램이 요청 연결에 `/bin/sh`를 연결 |

이 값들은 임의의 취약 서버에서 저절로 성립하지 않습니다. 메모리 배치가 시도마다 달라질 수 있어 고정 환경에서도 실패할 수 있으며, 스크립트는 기본 50회까지 새 프로세스로 재시도합니다. 공격 입력을 보낸 사실만으로 코드 실행을 입증하지 않고, 같은 연결에서 셸 명령 응답을 받아 성공을 판정합니다.

## 범위와 제한

PoC는 로컬 루프백에서만 동작합니다. 셸 명령은 Windows 호스트가 아니라 Docker 내부의 `node` 계정으로 실행되고, 연결을 닫으면 셸도 끝납니다. `/app/bridge`가 없는 일반 서버에 그대로 적용되는 범용 셸 페이로드는 아닙니다.

## 구성

| 경로 | 내용 |
| --- | --- |
| website/ | PoC 대상 Express 서버와 Docker 이미지 |
| exploit/exp.py | Docker 준비, Brotli 입력 생성, 셸 PoC와 연결 검증을 담은 단일 실행 파일 |
| compose.yaml | 로컬 PoC 대상 컨테이너 구성 |

## 참고 자료

- [Node.js 보안 공지](https://nodejs.org/en/blog/vulnerability/october-2026-security-releases)
- [Brotli 수정](https://github.com/google/brotli/commit/0ed4268a4401abcb4472096340477cf4af7d1a2b)
- [Express 5의 Brotli 요청 지원](https://github.com/expressjs/expressjs.com/blob/main/src/content/pages/en/guide/migrating-5.mdx)
- [Arkango의 MIT 연구](https://github.com/Arkango/CVE-2026-111497): exploit/exp.py의 Brotli 입력 생성 부분에 사용했으며, 저작권·허가 고지를 아래에 보존했습니다.
- [고정 바이너리 프로필 참고 연구](https://github.com/dimasma0305/CVE-2026-111497-POC): 수치를 참고했으며 코드는 복사하지 않았습니다.

## Arkango MIT 저작권 고지

MIT License

Copyright (c) 2026 Arkango

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
