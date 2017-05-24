<base href="/">
<?php
session_start();

$mysql_host = Redacted;
$mysql_database = Redacted;
$mysql_user = Redacted;
$mysql_password = Redacted;

//Create Connection
$conn = mysqli_connect($mysql_host,$mysql_user, $mysql_password, $mysql_database);

//Check Connection
if(!$conn) {
		die("Connection Failed: " . mysqli_connect_error());
}
?>