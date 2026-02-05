# Развертывание (Debian 12/13)

Ниже — минимальный набор скриптов для установки зависимостей, установки Mambaforge, создания окружений **ssg** и **esmfold**, а также сборки локальных C‑расширений проекта.

## 1) Системные зависимости (под root)

```bash
sudo bash deploy/00-system-deps-debian.sh
```

## 2) Установка Mambaforge (под обычным пользователем)

```bash
bash deploy/01-install-mambaforge.sh
```

По умолчанию ставится в `~/mambaforge`. При необходимости можно задать:

```bash
MAMBA_PREFIX=/opt/mambaforge bash deploy/01-install-mambaforge.sh
```

## 3) Создание окружений ssg и esmfold

CPU‑вариант (по умолчанию):

```bash
bash deploy/02-create-envs.sh
```

GPU‑вариант (CUDA):

```bash
ESMFOLD_CUDA=1 CUDA_VERSION=12.1 bash deploy/02-create-envs.sh
```

> Если у вас другая версия драйверов, измените `CUDA_VERSION`.

Для воспроизводимости версий ESMFold можно переопределить версии пакетов:

```bash
ESMFOLD_TORCH_VERSION=2.1.2 \
ESMFOLD_TORCHVISION_VERSION=0.16.2 \
ESMFOLD_TORCHAUDIO_VERSION=2.1.2 \
ESMFOLD_FAIR_ESM_VERSION=2.0.0 \
bash deploy/02-create-envs.sh
```

## 4) Установка локальных пакетов проекта в ssg

```bash
bash deploy/03-install-project.sh
```

## 5) Активация окружений

```bash
source deploy/activate_ssg.sh
source deploy/activate_esmfold.sh
```

## Примечания

- Скрипты ожидают Debian 12 или 13.
- Для работы `iupred3` нужен пакет `scipy` (устанавливается в `ssg`).
- Скрипты проекта импортируют `iupred3` как локальный пакет, поэтому запускать их нужно из корня репозитория.
