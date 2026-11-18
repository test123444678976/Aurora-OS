#!/usr/bin/env python3
"""aum.py - ferramenta para PC para ficheiros .aum (AuroraMemory) do AuroraOS.

  aum.py new FICHEIRO [MB]        cria um .aum vazio (por omissao 4 MB) para ligar a VM
  aum.py ls FICHEIRO              lista pastas e ficheiros guardados
  aum.py extract FICHEIRO PASTA   extrai tudo para uma pasta do teu computador
  aum.py pack PASTA FICHEIRO [MB] [--admin NOME:SENHA]
                                  empacota uma pasta do teu computador num .aum
  aum.py users FICHEIRO           lista as contas guardadas no .aum
  aum.py useradd FICHEIRO NOME SENHA [admin]   adiciona uma conta (ex.: criar o 1.o admin no PC)
  aum.py passwd FICHEIRO NOME NOVA_SENHA       repoe a palavra-passe (recuperacao)
  aum.py auto FICHEIRO on|off     liga/desliga a "memoria interna" (carregar e guardar automatico)

Formato (inteiros little-endian, sectores de 512 bytes):
  sector 0   "AUM1", versao (u32: 1 ou 2), tamanho do payload (u32), crc32 do payload (u32),
             numero de registos (u32), ano (u16), mes, dia, hora, minuto (u8 cada),
             numero de utilizadores (u8, so v2), flags (u8: bit 0 = memoria interna automatica)
  sector 1+  registos em profundidade: tipo (1=pasta, 2=ficheiro), pai (indice do registo; 255=raiz),
             tamanho do nome (u8), nome (Latin-1), tamanho dos dados (u32), dados
  v2: apos os registos, o bloco de utilizadores:
             n (u8) e n x [tam. nome u8, nome, admin u8, sal (8 bytes), hash (32 bytes)]
             hash = SHA256(sal+pw), depois 999 x SHA256(hash+sal+pw)   (pw em Latin-1, max. 39 bytes)
"""
import sys, os, struct, zlib, datetime, hashlib

NAME_MAX = 19          # o AuroraOS aceita nomes ate 19 caracteres
MAX_NODES = 63
MAX_BYTES = 2 * 1024 * 1024


def pwhash(salt, pw):
    pw = pw.encode("latin-1")[:39]
    h = hashlib.sha256(salt + pw).digest()
    for _ in range(999):
        h = hashlib.sha256(h + salt + pw).digest()
    return h


def read_aum(path, with_users=False):
    d = open(path, "rb").read()
    if len(d) < 512 or d[:4] != b"AUM1":
        raise SystemExit("%s: nao e um .aum com dados (cabecalho 'AUM1' em falta)" % path)
    ver, plen, crc, count = struct.unpack_from("<IIII", d, 4)
    y, mo, dd, hh, mi = struct.unpack_from("<HBBBB", d, 20)
    if ver not in (1, 2):
        raise SystemExit("versao %d nao suportada" % ver)
    payload = d[512:512 + plen]
    if len(payload) != plen or zlib.crc32(payload) & 0xFFFFFFFF != crc:
        raise SystemExit("CRC invalido: o .aum esta corrompido")
    recs, p = [], 0
    for _ in range(count):
        typ, parent, nl = struct.unpack_from("<BBB", payload, p); p += 3
        name = payload[p:p + nl].decode("latin-1"); p += nl
        (ln,) = struct.unpack_from("<I", payload, p); p += 4
        data = payload[p:p + ln]; p += ln
        recs.append((typ, parent, name, data))
    users = None
    if ver == 2:
        users = []
        n = payload[p]; p += 1
        for _ in range(n):
            nl = payload[p]; p += 1
            name = payload[p:p + nl].decode("latin-1"); p += nl
            admin = payload[p]; p += 1
            salt = payload[p:p + 8]; p += 8
            h = payload[p:p + 32]; p += 32
            users.append({"name": name, "admin": admin, "salt": salt, "hash": h})
    if with_users:
        return recs, (y, mo, dd, hh, mi), users
    return recs, (y, mo, dd, hh, mi)


def paths(recs):
    out = []
    for typ, parent, name, data in recs:
        base = "" if parent == 255 else out[parent]
        out.append(base + "/" + name)
    return out


def read_flags(path):
    d = open(path, "rb").read(512)
    return d[27] if d[:4] == b"AUM1" else 0


def write_aum(path, recs, mb=4, users=None, flags=0):
    payload = bytearray()
    for typ, parent, name, data in recs:
        nb = name.encode("latin-1")
        payload += struct.pack("<BBB", typ, parent, len(nb)) + nb + struct.pack("<I", len(data)) + data
    if users is not None:
        payload += bytes([len(users)])
        for u in users:
            nb = u["name"].encode("latin-1")
            payload += bytes([len(nb)]) + nb + bytes([1 if u["admin"] else 0]) + u["salt"] + u["hash"]
    n = datetime.datetime.now()
    hdr = bytearray(512)
    hdr[0:4] = b"AUM1"
    hdr[26] = len(users) if users is not None else 0
    hdr[27] = flags
    struct.pack_into("<IIII", hdr, 4, 2 if users is not None else 1, len(payload), zlib.crc32(bytes(payload)) & 0xFFFFFFFF, len(recs))
    struct.pack_into("<HBBBB", hdr, 20, n.year, n.month, n.day, n.hour, n.minute)
    hdr[32:32 + 30] = b"AuroraMemory (.aum) - AuroraOS"
    total = max(mb * 1024 * 1024, 512 + len(payload))
    total += (-total) % 512
    with open(path, "wb") as f:
        f.write(hdr); f.write(payload); f.write(b"\0" * (total - 512 - len(payload)))


def cmd_new(a):
    mb = int(a[1]) if len(a) > 1 else 4
    with open(a[0], "wb") as f:
        f.truncate(mb * 1024 * 1024)
    print("criado %s (%d MB, vazio)" % (a[0], mb))


def cmd_ls(a):
    recs, when, users = read_aum(a[0], True)
    print("%s: %d itens, %s, guardado em %02d/%02d/%d %02d:%02d" % (a[0], len(recs),
          "sem bloco de utilizadores (v1)" if users is None else "%d utilizadores" % len(users), when[2], when[1], when[0], when[3], when[4]))
    for (typ, _, _, data), p in zip(recs, paths(recs)):
        print("  %-40s %s" % (p + ("/" if typ == 1 else ""), "<pasta>" if typ == 1 else "%d bytes" % len(data)))


def cmd_extract(a):
    recs, _ = read_aum(a[0])
    out = a[1]
    os.makedirs(out, exist_ok=True)
    for (typ, _, _, data), p in zip(recs, paths(recs)):
        dest = os.path.join(out, p.lstrip("/"))
        if typ == 1:
            os.makedirs(dest, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, "wb").write(data)
    print("extraidos %d itens para %s" % (len(recs), out))


def cmd_pack(a):
    users = None
    for i, x in enumerate(list(a)):
        if x == "--admin":
            name, pw = a[i + 1].split(":", 1)
            salt = os.urandom(8)
            users = [{"name": name, "admin": 1, "salt": salt, "hash": pwhash(salt, pw)}]
            del a[i:i + 2]
            break
    src = a[0]
    mb = int(a[2]) if len(a) > 2 else 4
    recs, total = [], 0

    def walk(d, parent):
        nonlocal total
        names = sorted(os.listdir(d), key=lambda n: (not os.path.isdir(os.path.join(d, n)), n))
        for n in names:
            full = os.path.join(d, n)
            if len(n) > NAME_MAX or "/" in n:
                print("ignorado (nome > %d caracteres): %s" % (NAME_MAX, full)); continue
            try:
                n.encode("latin-1")
            except UnicodeEncodeError:
                print("ignorado (caracteres fora de Latin-1): %s" % full); continue
            if len(recs) >= MAX_NODES:
                raise SystemExit("demasiados itens (maximo %d)" % MAX_NODES)
            if os.path.isdir(full):
                recs.append((1, parent, n, b"")); me = len(recs) - 1
                walk(full, me)
            else:
                data = open(full, "rb").read()
                total += len(data)
                if total > MAX_BYTES:
                    raise SystemExit("dados a mais (maximo 2 MB)")
                recs.append((2, parent, n, data))
    walk(src, 255)
    write_aum(a[1], recs, mb, users)
    print("criado %s com %d itens%s" % (a[1], len(recs), " e 1 administrador" if users else ""))


def cmd_users(a):
    recs, _, users = read_aum(a[0], True)
    if users is None:
        print("%s: formato v1 (sem utilizadores)" % a[0]); return
    for u in users:
        print("  %-14s %s" % (u["name"], "administrador" if u["admin"] else "utilizador"))
    if not users:
        print("  (nenhum: o AuroraOS vai pedir para criar o administrador)")


def _modify(path, fn):
    flags = read_flags(path)
    recs, _, users = read_aum(path, True)
    users = [] if users is None else users
    fn(users)
    write_aum(path, recs, max(1, os.path.getsize(path) // (1024 * 1024)), users, flags)


def cmd_useradd(a):
    name, pw = a[1], a[2]
    admin = len(a) > 3 and a[3] == "admin"
    if not (1 <= len(name) <= 12 and all(c.isalnum() and c.isascii() or c in "_-" for c in name)):
        raise SystemExit("nome invalido (1-12 letras/numeros/-/_)")
    if len(pw) < 4:
        raise SystemExit("a palavra-passe precisa de pelo menos 4 caracteres")

    def fn(users):
        if any(u["name"].lower() == name.lower() for u in users):
            raise SystemExit("esse utilizador ja existe")
        if len(users) >= 8:
            raise SystemExit("limite de 8 utilizadores")
        salt = os.urandom(8)
        users.append({"name": name, "admin": 1 if admin else 0, "salt": salt, "hash": pwhash(salt, pw)})
    _modify(a[0], fn)
    print("conta '%s' adicionada (%s)" % (name, "administrador" if admin else "utilizador"))


def cmd_passwd(a):
    name, pw = a[1], a[2]
    if len(pw) < 4:
        raise SystemExit("a palavra-passe precisa de pelo menos 4 caracteres")

    def fn(users):
        for u in users:
            if u["name"].lower() == name.lower():
                u["salt"] = os.urandom(8); u["hash"] = pwhash(u["salt"], pw); return
        raise SystemExit("utilizador nao encontrado")
    _modify(a[0], fn)
    print("palavra-passe de '%s' alterada" % name)


def cmd_auto(a):
    on = a[1].lower() in ("on", "1", "sim")
    d = bytearray(open(a[0], "rb").read())
    if d[:4] == b"AUM1":
        d[27] = 1 if on else 0                   # as flags nao entram no CRC
        open(a[0], "wb").write(d)
    elif not any(d[:8192]):                      # disco novo: cria uma memoria vazia ja com o modo
        write_aum(a[0], [], max(1, len(d) // (1024 * 1024)), [], 1 if on else 0)
    else:
        raise SystemExit("%s nao parece um .aum (nem esta em branco): recusado" % a[0])
    print("memoria interna %s em %s" % ("ligada" if on else "desligada", a[0]))


def main():
    cmds = {"new": cmd_new, "ls": cmd_ls, "extract": cmd_extract, "pack": cmd_pack,
            "users": cmd_users, "useradd": cmd_useradd, "passwd": cmd_passwd, "auto": cmd_auto}
    if len(sys.argv) < 3 or sys.argv[1] not in cmds:
        print(__doc__); sys.exit(1)
    cmds[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    main()
