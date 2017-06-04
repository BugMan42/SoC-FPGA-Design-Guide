LIBRARY IEEE;
USE IEEE.STD_LOGIC_1164.ALL;
use ieee.numeric_std.all;

ENTITY merge2 IS
	PORT (in2_1, in2_2, in2_3, in2_4: IN unsigned (31 DOWNTO 0) := X"00000000";
			out2_1, out2_2, out2_3, out2_4: OUT unsigned(31 DOWNTO 0) := X"00000000");
END merge2;

ARCHITECTURE level2 OF merge2 IS
	--SIGNAL flag: std_logic;

BEGIN
	PROCESS (in2_1, in2_2, in2_3, in2_4) IS
		variable flag : std_logic := '0';
	BEGIN
		flag := '0';
		IF (in2_1 < in2_3) THEN
			 out2_1 <= in2_1;
		ELSE
			 out2_1 <= in2_3;
			 flag := '1';
		end if;
		IF (in2_2 < in2_3 and flag = '0') THEN
			 out2_2 <= in2_2;
			 out2_3 <= in2_3;
			 out2_4 <= in2_4;
		ELSIF (in2_2 < in2_4 and (flag = '0')) THEN
			 out2_2 <= in2_3;
			 out2_3 <= in2_2;
			 out2_4 <= in2_4;
		ELSIF (flag = '0') THEN
			 out2_2 <= in2_3;
			 out2_3 <= in2_4;
			 out2_4 <= in2_2;
		END IF;
		IF (in2_4 < in2_1 and (flag = '1')) THEN
			 out2_2 <= in2_4;
			 out2_3 <= in2_1;
			 out2_4 <= in2_2;
		ELSIF (in2_4 < in2_2 and (flag = '1')) THEN
			 out2_2 <= in2_1;
			 out2_3 <= in2_4;
			 out2_4 <= in2_2;
		ELSIF (flag = '1') THEN
			 out2_2 <= in2_1;
			 out2_3 <= in2_2;
			 out2_4 <= in2_4;
		END IF;
	END PROCESS;
END level2;