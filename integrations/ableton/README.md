# SLOOP + Ableton Live — 6T8

Este pacote reúne os templates do Live, os clips de seção A–D e o dispositivo **SLOOP Live Recall**. Use o firmware PEEEO 6T8 para os comandos de seção. O dispositivo requer Max for Live, incluído no Live Suite.

## Preparar o FM-1 e o Live

1. No FM-1, deixe os canais das seis tracks em **1, 2, 3, 4, 5 e 10**. Em **GLO → MIDI 3**, escolha **REMOTE = CH16** e **DR MAP = GM**. Reserve CH16 para controle.
2. No Live, habilite **Track** na entrada e na saída **Felucca**. Habilite **Sync somente na saída**, com **MIDI Clock Type = Song**. Estes ajustes pertencem às preferências do Live e precisam ser feitos uma vez no computador.
3. Escolha a entrada de áudio do FM-1 nas preferências de áudio do Live. A pista `SLOOP Audio` recebe **Ext. In 1/2**, em estéreo: o USB entrega a mistura das seis tracks.
4. Abra um dos templates abaixo. Se sua porta tiver outro nome, selecione-a nos campos MIDI From/To. Seus templates anteriores não são substituídos.

## Templates

| Arquivo | Uso | FM-1 |
| --- | --- | --- |
| `SLOOP DAW.als` | Tocar/gravar pelo Live; seis pistas com entrada e saída no mesmo canal | GLO → DAW → MODE **DAW** |
| `SLOOP Capture.als` | Gravar o sequencer/arp/rolls do aparelho nas seis pistas | MODE **CAPT** |

Arme as pistas musicais que vai gravar. No template DAW, o Monitor Auto devolve as notas da pista armada ao aparelho; as teclas do FM-1 ficam sem som local nesse perfil. Para um teclado externo, selecione a porta/canal desse teclado na entrada da pista que deseja tocar; mantenha a saída Felucca no canal da track correspondente.

O template Capture usa Monitor Off e MIDI To None nas seis pistas musicais. Assim a captura não retorna notas ao aparelho. Para reproduzir a gravação pelos sons do FM-1, mude MODE para DAW e selecione Felucca/CH1–5 ou CH10 nas saídas das pistas gravadas.

Há duas pistas auxiliares: `SLOOP Sections` para os clips A–D e `SLOOP Recall` para guardar os sons. O retorno estéreo é uma pista de áudio separada. `Drum Map.csv` lista os 16 sons e seus números MIDI; a numeração evita diferenças de nome das oitavas entre programas.

## Trocar seções pelos clips

Salve conteúdo nas seções A–D do aparelho e use o modo LOOP. A pista `SLOOP Sections` contém quatro clips sem notas, com **CC119 = 0, 1, 2 ou 3**, saindo no **CH16**.

Durante a reprodução, o SLOOP aplica a última seção pedida no próximo compasso. Os clips usam Launch Quantization **None**, pois o aparelho faz a quantização; deixá-los em 1 Bar adicionaria outra espera. Com o aparelho parado, um CC119 válido carrega a seção sem iniciar PLAY. Uma seção vazia ou um valor acima de 3 é ignorado. Uma sequência SONG em andamento não é interrompida. STOP e reposicionamento por SPP cancelam pedidos pendentes.

Para automatizar no Arrangement, copie os clips para essa mesma pista. Também pode criar um clip vazio, escolher MIDI Ctrl → **119** no editor de envelopes e definir um valor de 0 a 3. Program Change continua escolhendo presets nas pistas musicais.

## Guardar os sons no Live Set

1. Mantenha `SLOOP Live Recall.amxd` e `sloop-live.js` na mesma pasta. Arraste o `.amxd` para a pista `SLOOP Recall`.
2. Nessa pista, mantenha **MIDI From = Felucca / All Channels**, **MIDI To = Felucca / All Channels** e mude **Monitor para In**. A pista vem em Off até receber o dispositivo, para evitar retorno de notas por uma pista vazia.
3. Pare os transportes do Live e do FM-1. Prepare os sons e aperte **CAPTURE**. Aguarde `CAPTURE OK` e salve o Live Set com **Cmd+S**.
4. Ao reabrir esse Set com o aparelho conectado e os transportes parados, **Restaurar ao abrir** recupera o snapshot. Aguarde `RESTORE OK` antes de tocar. O botão **RESTORE** faz a mesma operação manualmente.
5. Se a porta não estiver disponível ao abrir, a operação informa a falta de resposta; conecte/configure o FM-1 e use RESTORE. Desmarque a opção automática para escolher quando restaurar. **CANCEL** interrompe a operação sem apagar o snapshot salvo.

O snapshot fica no próprio Live Set, por um parâmetro Blob do Max. Mantenha também a pasta do dispositivo com o projeto para que o Live encontre o código ao reabri-lo ou ao transportá-lo para outro computador. Após nova edição dos sons, repita CAPTURE e salve o Set: a captura é explícita, não contínua.

São guardados os engines e parâmetros dos cinco synths, o patch de trabalho completo de cada track FM6, o kit/mute, a afinação e o decay dos 16 sons da bateria, a mistura e os efeitos globais de som. A restauração usa confirmações do aparelho, uma escrita por vez. Não altera BPM, canais MIDI, perfil DAW, padrões ou seções A–D. Presets de usuário recuperam seus valores sonoros; a indicação do nome/slot de usuário não faz parte deste protocolo.

Samples e bancos de kits de usuário continuam sendo recursos do aparelho: mantenha esses recursos ou um backup separado se for trocá-los. O Recall não instala samples nem regrava bancos em flash. Restaurar pode levar alguns segundos; se o transporte começar ou a conexão cair, a operação para e pode ter aplicado parte dos sons. Pare e repita RESTORE para concluir.

O dispositivo filtra notas e controles musicais recebidos nessa pista. Se remover ou desativar o dispositivo, volte o Monitor da pista Recall para Off para que a pista vazia não devolva notas.

## Checklist no aparelho

- [ ] Em CAPT, grave as seis tracks nos canais **1, 2, 3, 4, 5 e 10**; confira notas e automações de cada uma.
- [ ] Em DAW, toque um teclado em cada pista/canal e confira synth 5 e bateria, sustain, bend e Program Change.
- [ ] Confira todos os 16 números de `Drum Map.csv` com DR MAP = GM.
- [ ] Salve quatro seções diferentes; dispare A–D no meio do compasso e confira a troca no compasso seguinte.
- [ ] Com notas longas na track 5 e na bateria, troque seções e aperte STOP; nenhuma nota deve ficar presa.
- [ ] Peça uma seção e aperte STOP antes da troca; ela não deve entrar no próximo PLAY.
- [ ] Capture sons editados, incluindo FM6; salve/feche o Set, altere os sons no aparelho e reabra. Compare os sons após RESTORE OK.
- [ ] Confira que os padrões, canais e BPM não foram substituídos pelo Recall.
- [ ] Desconecte durante uma captura: o snapshot anterior deve continuar disponível. Reconecte e repita RESTORE com STOP.
- [ ] Restaure um backup antigo: confira os canais novos, os defaults de GLO → DAW e REMOTE; configure CH16 novamente se o backup o deixar em OFF.

## Validação desta entrega

Compilação do firmware, memória, integridade do instalador e testes automatizados de roteamento, transporte, seções, serialização e protocolo do Recall foram verificados no computador. Os arquivos nativos do Live e do Max passaram por verificações de estrutura. A abertura/renderização do dispositivo no Live e o teste físico com FM-1 ainda precisam de validação; esta primeira integração deve ser tratada como experimental.

Para gerar novamente os arquivos no macOS com Live 12 Suite instalado: `python3 tools/build_live_bundle.py`. Testes: `python3 tests/live_bundle_test.py` e `node tests/live_recall_test.mjs`.
