# esercizio-C-2020-05-19-procs


N = 10

un processo padre crea N processi figli ( https://repl.it/@MarcoTessarotto/crea-n-processi-figli )

shared variables: countdown, process_counter[N], shutdown

usare mutex per regolare l'accesso concorrente a countdown

dopo avere avviato i processi figli, il processo padre dorme 1 secondo e poi
imposta il valore di countdown al valore 100000.

quando countdown == 0, il processo padre imposta shutdown a 1.

aspetta che terminino tutti i processi figli e poi stampa su stdout process_counter[].



i processi figli "monitorano" continuamente countdown:

- processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

- se shutdown != 0, processo i-mo termina
