// Usa segredos.h se existir; senão, os valores de exemplo (o build da CI usa estes).
#pragma once

#if __has_include("segredos.h")
#include "segredos.h"
#else
#warning "segredos.h nao encontrado: usando segredos.exemplo.h. Copie e preencha antes de gravar na caixa."
#include "segredos.exemplo.h"
#endif
