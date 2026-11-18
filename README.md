# AuroraOS

Sistema operativo feito de raiz: bootloader (assembly), kernel (C, 32 bits), interface gráfica.
Sem Linux, sem libc, sem GRUB.

## Compilar
`./build.sh`  (precisa de gcc, as, ld, objcopy, python3) -> `aurora.iso`

## Correr (QEMU)
    ./run.sh        # (Windows: run.bat) cria memoria.aum se faltar e arranca com ela

Ou à mão:
    python3 tools/aum.py new memoria.aum          # uma vez: cria o disco de memória (.aum)
    qemu-system-i386 -m 64 -boot d -cdrom aurora.iso \
        -drive file=memoria.aum,format=raw,if=ide,index=1

Sem o `-drive`, o sistema funciona na mesma, mas a memória perde-se ao desligar.

## Utilizadores e login
* No arranque aparece o **ecrã de login**. Na primeira utilização (sem contas) cria-se a conta de administrador.
* As contas ficam guardadas no .aum (formato v2). Palavras-passe: sal aleatório + SHA-256 iterado 1000x.
* Cada utilizador tem uma pasta pessoal privada em `/home/<nome>`; os outros utilizadores não a veem.
  Os administradores veem tudo, criam/apagam contas e podem carregar um .aum.
* App **Utilizadores** (ou o nome no canto da barra de tarefas): criar contas, mudar palavra-passe, dar/tirar admin.
* Terminal: `whoami`, `users`, `logout`. Menu Aurora: Terminar sessão.
* No PC: `tools/aum.py users|useradd|passwd` (ex.: recuperar uma palavra-passe esquecida, ou criar o 1.º admin).
* Ordem no arranque: pergunta pelo .aum (se existir) e depois o login com as contas carregadas.

## Memória interna (automática)
* O .aum ligado à máquina pode funcionar como **memória interna**: no primeiro arranque com um disco novo o
  sistema pergunta se o queres usar assim. Ligada, a memória **carrega sozinha no arranque** (sem perguntas)
  e **guarda sozinha** 4 s depois da última alteração (e ao reiniciar/desligar).
* App Memória: interruptor "Memória interna" (ou terminal: `auto on|off`). Só administradores o mudam.
* LED na barra de tarefas: verde = guardada, laranja = por guardar, vermelho = erro (auto-guardar suspenso).
* Se a memória interna estiver ilegível (CRC), o auto-guardar fica desligado para não a sobrescrever.
* No PC: `tools/aum.py auto memoria.aum on` liga o modo sem arrancar a VM.

## Memória (.aum)
* App **Memória** (ou terminal: `save`, `load`, `aum`): guarda/carrega todos os ficheiros e pastas.
* No arranque, se houver um .aum com dados, o sistema pergunta se o quer carregar.
* Ao reiniciar/desligar com alterações por guardar, pergunta se quer guardar.
* O .aum também guarda as contas de utilizador.
* O .aum é um ficheiro normal no teu PC. `tools/aum.py ls|extract|pack` permite ver o conteúdo,
  tirar tudo para uma pasta, ou empacotar uma pasta do PC para dentro do .aum.
* Segurança: só se escreve em discos .aum ou totalmente em branco. Qualquer outro disco é ignorado.
* Formato: ver o cabeçalho de `tools/aum.py` e de `src/aum.c`.
