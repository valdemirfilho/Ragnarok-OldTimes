
CREATE TABLE  `harmony_log` (
  `log_id` INT NOT NULL AUTO_INCREMENT PRIMARY KEY ,
  `account_id` INT NOT NULL ,
  `char_name` VARCHAR( 24 ) NOT NULL ,
  `date` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ,
  `IP` VARCHAR( 20 ) NOT NULL ,
  `data` VARCHAR( 200 ) NOT NULL ,
  INDEX (  `account_id` )
) ENGINE = MYISAM CHARACTER SET utf8 COLLATE utf8_general_ci;

ALTER TABLE `loginlog` ADD `mac` VARCHAR( 18 ) NOT NULL DEFAULT '';
