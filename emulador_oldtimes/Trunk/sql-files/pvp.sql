-- OBS: Preste aten��o nos updates, eles s�o por vers�o.

-- v1.0.0+
DROP TABLE IF EXISTS `pvpm_data`;
CREATE TABLE `pvpm_data` (
`id` int(11) unsigned NOT NULL default '0',
`name` varchar(255) NOT NULL default '',
`kills` int(11) unsigned NOT NULL default '0',
`dies` int(11) unsigned NOT NULL default '0',
`leader` int(11) unsigned NOT NULL default '0',
`annc` int(11) unsigned NOT NULL default '0',
`ban` int(11) unsigned NOT NULL default '0',
`type` int(11) unsigned NOT NULL default '0',
KEY `id` (`id`),
KEY `kills` (`kills`),
KEY `dies` (`dies`)
) ENGINE=MyISAM;

-- v1.1.1+
alter table pvpm_data add ratio int(11) not null default '0' after dies;

-- v1.1.7+
DROP TABLE IF EXISTS `pvpm_map`;
CREATE TABLE `pvpm_map` (
`id` int(11) unsigned NOT NULL default '0',
`map` varchar(255) NOT NULL default 'NULL',
`type` int(11) unsigned NOT NULL default '0',
KEY `id` (`id`)
) ENGINE=MyISAM;
insert into pvpm_map (id,map,type) values (0,'guild_vs3',0);
insert into pvpm_map (id,map,type) values (1,'pvp_n_5-5',0);
insert into pvpm_map (id,map,type) values (2,'guild_vs1',2);
insert into pvpm_map (id,map,type) values (3,'prt_are01',2);
insert into pvpm_map (id,map,type) values (4,'guild_vs2',1);

-- v1.2.3+
alter table pvpm_map add noguild int(11) not null default '0';
update pvpm_map set noguild=1 where map='guild_vs1';

-- v1.3.0+

DROP TABLE IF EXISTS `pvpm_lang`;
CREATE TABLE IF NOT EXISTS `pvpm_lang` (
  `id` int(11) unsigned NOT NULL,
  `lang` int(11) unsigned NOT NULL default '0',
  `str` varchar(255) NOT NULL default '',
  KEY `id` (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- Tabela para mensagens do PvP Master v1.3.0+ #Portugu�s.

DELETE FROM `pvpm_lang` where lang = 0;

INSERT INTO `pvpm_lang` (`id`, `lang`, `str`) VALUES
(0, 0, 'Portugu�s'),
(1, 0, 'Bem vindo as Arenas de luta!'),
(2, 0, 'Nestas Arenas voc� pode duelar com outros jogadores!'),
(3, 0, 'Os atuais L�deres das Arena s�o:'),
(4, 0, 'L�der'),
(5, 0, 'Entrar na Arena'),
(6, 0, 'Comandos'),
(7, 0, 'Sair'),
(8, 0, '(Mortes)'),
(9, 0, 'Minha Coloca��o'),
(10, 0, 'O sistema de comandos remotos funciona da seguinte mandeira:'),
(11, 0, '- Envie uma PM(Player Message) para  ^008B8Bnpc:cmd.'),
(12, 0, '- Esse � o nome do npc. Ele automaticamente retornar� uma lista de comandos para voc�.'),
(13, 0, '<!!!> Voc� deve informar o n�mero da Arena.'),
(14, 0, '<!!!> Voc� ser� retirado da Arena em 5 Segundos . . .'),
(15, 0, '<!!!> Est� fun��o s� pode ser usada dentro da Arena.'),
(16, 0, '<!!!> Ranking do foi limpo.'),
(17, 0, '<!!!> Voc� deve especificar qual arena deseja resetar o Ranking.'),
(18, 0, '� necess�rio que o Nome seja informado.'),
(19, 0, '� necess�rio que o Tempo seja informado.'),
(20, 0, '� necess�rio que o Tipo de banimento seja informado.'),
(21, 0, '<!!!> Tempo de(o): '),
(22, 0, '<!!!> O banimento foi retirado de(o) '),
(23, 0, '[GM Mensagem] Voc� foi desbanido da Arena PvP.'),
(25, 0, '<!!!> Ningu�m foi encontrado com este nome.'),
(26, 0, '<!!!> Tempo de banimento alterado.'),
(27, 0, 'Minutos'),
(28, 0, '[GM Mensagem] Voc� est� banido da Arena PvP por'),
(24, 0, 'O valor do Tempo n�o � v�lido, por favor utilize caracteres de 0 � 9.'),
(29, 0, '<!!!> O sistema de arenas n�o suporta mais de 10 mapas.'),
(30, 0, '<!!!> Se quiser alterar o valor m�ximo, entre em contado com Mooka. (mooka.support@hotmail.com)'),
(31, 0, '<!!!> 1. Por favor, defina o tipo de arena.'),
(32, 0, '<!!!> 2. S� � permitido o uso dos caracteres de 0 � 2.'),
(33, 0, '<!!!> - 0 = Arena PvP; 1 = Arena PvP Baby; 2 Arena GvG;'),
(34, 0, '<!!!> Esse mapa j� est� sendo usado pelo [PvP Master].'),
(35, 0, '<!!!> O mapa foi adicionado. '),
(36, 0, '<!!!> Opa! Voc� n�o pode retirar mais mapas do PvP Master, isto causar� problemas!'),
(37, 0, '<!!!> Nenhum mapa com este nome est� sendo usado.'),
(38, 0, '<!!!> O mapa foi retirado.'),
(39, 0, '<!!!>'),
(40, 0, 'O comando ''!pvp'' serve para teleport�-lo � arena desejada, para us�-lo voc� deve enviar a mensagem:'),
(41, 0, 'Ap�s o ''#'' � o n�mero da arena.'),
(42, 0, '!ranking serve para mostrar o Ranking. Para isso, voc� deve enviar uma das mensagens abaixo:'),
(43, 0, 'Voc� deve especificar qual ranking voc� deseja visualizar (#pvp ou #gvg), caso deseje o ranking de mortes adicione #die no final.'),
(44, 0, '!pvplider ou !gvglider serve para mostrar o player ou guild com mais pontos.'),
(45, 0, 'Serve para retir�-lo da arena, teleportando-o para seu savepoint.'),
(46, 0, 'Este comando ir� banir determinado player/guild pelo nome, o tempo � em minutos.'),
(47, 0, 'Exemplo: !ban#nome#tempo(em minutos)#0(Player)/2(Guild) -> !ban#Player1#60#1 / !ban#EmpBreak#60#2'),
(48, 0, 'Voc� deve definir se est� banindo um player ou guild, colocando ao final 1 para Player e 2 para Guild.'),
(49, 0, 'Para diminuir o tempo, utilize o sinal de menos(-) antes do valor. Para desbanir o mesmo, envie no campo de Tempo ''unban''.'),
(50, 0, 'Caso deseje ver o tempo restante, envie no campo do Tempo ''showtime''.'),
(51, 0, 'Comando para adicionar ou retirar mapas.'),
(52, 0, '-> !map#add#mapname#type para adicionar um mapa.'),
(53, 0, '-> !map#del#mapname para deletar um mapa.'),
(54, 0, 'Comando para limpar a tabela do ranking.'),
(55, 0, '-> !truncate#pvp para limpar o ranking do PvP.'),
(56, 0, '-> !truncate#gvg para limpar o ranking do GvG.'),
(57, 0, '<!!!> Nenhum comando foi encontrado.'),
(58, 0, '- Lista De Comandos -'),
(59, 0, '--- Use !help#!COMANDO para mais informa��es. ---'),
(60, 0, '- !warp#n�mero (Teleporta-o para Arena)'),
(61, 0, '- !ranking (Ranking normal)'),
(62, 0, '- !ranking2 (Mostra sua posi��o no Ranking)'),
(63, 0, '- !pvplider (Mostra o lider do PvP)'),
(64, 0, '- !gvglider (Mostra o lider do GvG)'),
(65, 0, '- !exit (Sai da arena)'),
(66, 0, '--- GM''s Comandos ---'),
(67, 0, '- !truncate (Limpa o Ranking)'),
(68, 0, '- !ban (Bani o player da arena)'),
(69, 0, '- !map (Add ou Del um mapa do PvP Master)'),
(70, 0, '<!!!> Voc� n�o est� entre os 125 colocados do Ranking.'),
(71, 0, 'Desculpe, mas voc� precisa estar em uma Guild para entrar nesta Arena.'),
(72, 0, 'Olhe o seu tamanho! Esta arena � restrita a Baby''s!'),
(73, 0, 'Voc� n�o pode usar est� fun��o dentro da Arena.'),
(74, 0, 'Voc� est� banido da Arena por'),
(75, 0, '� o Novo Lider'),
(76, 0, 'Pontos'),
(77, 0, 'Voc� Ganhou:'),
(78, 0, 'Voc� Perdeu:'),
(79, 0, 'Linguagem'),
(80, 0, 'Selecione sua linguagem:'),
(81, 0, 'Valor inv�lido.');
