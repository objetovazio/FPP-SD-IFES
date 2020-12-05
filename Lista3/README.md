# Lista 1

Faca um somatorio de um vetor imenso com multiplas threads [tudo deve ser passado por argumento] SEM USAR NENHUMA VARIAVEL GLOBAL,  acumule tudo na main para minimizar as condicoes de corrida.

Alem disso, coloque todas as threads para COMECAREM paradas esperando num semaforo para somente quando a thread mais lerda pegar o ultimo sinal do semaforo e assim, essa ultima thread libere todas as outras threads para SOMENTE AI comecarem a somar tudo. [a main continua esperando no join sem muita alteracao]

Reparem que assim eh  o meio mais CORRETO de medir tempo em threads, pra comparar com o serial, pois vc mantem todas as threads na linha de largada e somente quando a ultima ocupar o sinalizador, eh que tudo comece ao "mesmo tempo" , fazendo assim ser um fator importante para calcular SPEEDUP