select s.fname, s.lname, value, c.course_title from students s
join grades g on g.id_stud=s.id_stud
join courses c on c.id_course=g.id_course
where value in (select max(value) from students s
join grades g on g.id_stud=s.id_stud
join courses c on c.id_course=g.id_course 
group by c.course_title
);

